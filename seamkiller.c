//CC0 1.0 Universal https://creativecommons.org/publicdomain/zero/1.0/
//To the extent possible under law, Blackle Mori has waived all copyright and related or neighboring rights to this work.

/* This plugin performs a periodic + smooth decomposition on 
 * the input to remove the seams that appear when tiling.
 */

#define GETTEXT_PACKAGE "gtk20"
#include <glib/gi18n-lib.h>
#include <math.h>

#ifdef GEGL_PROPERTIES

property_int  (samples, _("RWOS samples"), 100)
  value_range (1, 500)
  ui_range    (0, 500)
  description (_("Number of RWOS samples"))

property_int  (steps, _("RWOS steps"), 50)
  value_range (1, 100)
  ui_range    (0, 100)
  description (_("Number of RWOS steps"))

property_seed (seed, _("Random seed"), rand)

#else

#define GEGL_OP_AREA_FILTER
#define GEGL_OP_NAME     seamkiller
#define GEGL_OP_C_SOURCE seamkiller.c

#include "gegl-op.h"

static GeglRectangle
get_effective_area (GeglOperation *operation)
{
  GeglRectangle  result = {0,0,0,0};
  GeglRectangle *in_rect = gegl_operation_source_get_bounding_box (operation, "input");

  gegl_rectangle_copy(&result, in_rect);

  return result;
}


static void
random_unit_vector(const GeglRandom *rand, gint x, gint y, gint* n, gfloat* rx, gfloat* ry)
{
  gfloat angle = gegl_random_float(rand, x, y, 0, (*n)++) * 2 * 3.14159265f;
  *rx = cos(angle);
  *ry = sin(angle);
}

static gfloat
distance_to_rectangle(const GeglRectangle* rect, gfloat x, gfloat y)
{
  gfloat xdist = fmin(x - rect->x, rect->x + rect->width - x);
  gfloat ydist = fmin(y - rect->y, rect->y + rect->height - y);
  return fmin(ydist, xdist);
}

static void
swap(gfloat* a, gfloat* b)
{
  gfloat tmp = *a;
  *a = *b;
  *b = tmp;
}

static gboolean
closest_and_antipode_on_rectangle(const GeglRectangle* rect, gfloat x, gfloat y, gfloat* cx, gfloat* cy, gfloat* ax, gfloat* ay)
{
  *cx = *ax = x; *cy = *ay = y;
  gfloat xdist1 = x - rect->x;
  gfloat xdist2 = rect->x + rect->width - x;
  gfloat xdist = fmin(xdist1, xdist2);

  gfloat ydist1 = y - rect->y;
  gfloat ydist2 = rect->y + rect->height - y;
  gfloat ydist = fmin(ydist1, ydist2);
  if (xdist < ydist) {
    *cx = rect->x;
    *ax = rect->x + rect->width;
    if (xdist1 > xdist2) {
      swap(cx, ax);
    }
    return TRUE;
  }
  *cy = rect->y;
  *ay = rect->y + rect->height;
  if (ydist1 > ydist2) {
    swap(cy, ay);
  }
  return FALSE;
}

static void
rwos_sample(GeglSampler *sampler,
            const GeglRandom *rand,
            const GeglRectangle* rect,
            const gint steps,
            const gint x,
            const gint y,
            gint* n,
            gfloat out[4])
{
  gfloat sx = x, sy = y, dist;
  for (gint i = 0; i < steps; i++)
  {
    gfloat rx, ry;
    random_unit_vector(rand, x, y, n, &rx, &ry);
    dist = distance_to_rectangle(rect, sx, sy);
    sx += dist * rx;
    sy += dist * ry;
    if (dist < 1) break;
  }
  gfloat cx, cy, ax, ay;
  closest_and_antipode_on_rectangle(rect, sx, sy, &cx, &cy, &ax, &ay);
  GeglBufferMatrix2  scale;

  gfloat closest[4];
  gfloat antipode[4];
  gegl_sampler_get (sampler, cx, cy, &scale, closest, GEGL_ABYSS_CLAMP);
  gegl_sampler_get (sampler, ax, ay, &scale, antipode, GEGL_ABYSS_CLAMP);

  for (gint i=0; i<4; i++)
    out[i] = (closest[i]-antipode[i]) / 2;
}

static gboolean
process (GeglOperation       *operation,
         GeglBuffer          *input,
         GeglBuffer          *output,
         const GeglRectangle *result,
         gint                 level)
{
  GeglProperties *o = GEGL_PROPERTIES (operation);

  GeglRectangle            boundary     = get_effective_area (operation);
  const Babl              *format       = gegl_operation_get_format (operation, "output");
  GeglSampler             *sampler      = gegl_buffer_sampler_new_at_level (
                                    input, format, GEGL_SAMPLER_NEAREST,
                                    level);

  gint      x,y;
  gfloat   *src_buf, *dst_buf;
  gfloat    dest[4];
  gint      i, j, offset = 0;
  GeglRandom* rand = gegl_random_new_with_seed(o->seed);

  gint samples = o->samples;
  gint steps = o->steps;

  src_buf = g_new0 (gfloat, result->width * result->height * 4);
  dst_buf = g_new0 (gfloat, result->width * result->height * 4);

  gegl_buffer_get (input, result, 1.0, format, src_buf, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_CLAMP);
  
  GeglBufferMatrix2  scale;

  for (y = result->y; y < result->y + result->height; y++)
    for (x = result->x; x < result->x + result->width; x++)
      {
        gint n = 0;
        gegl_sampler_get (sampler, x, y, &scale, dest, GEGL_ABYSS_CLAMP);

        for (i = 0; i < samples; i++) {
          gfloat smp[4];
          rwos_sample(sampler, rand, &boundary, steps, x, y, &n, smp);

          for (j=0; j<4; j++)
            dest[j] -= smp[j] / samples;
        }

        for (i=0; i<4; i++)
          dst_buf[offset++] = dest[i];
      }

  gegl_buffer_set (output, result, 0, format, dst_buf, GEGL_AUTO_ROWSTRIDE);

  g_free (src_buf);
  g_free (dst_buf);

  g_object_unref (sampler);
  gegl_random_free (rand);

  return  TRUE;
}

static GeglRectangle
get_required_for_output (GeglOperation       *operation,
                         const gchar         *input_pad,
                         const GeglRectangle *roi)
{
  const GeglRectangle *in_rect =
            gegl_operation_source_get_bounding_box (operation, "input");

  if (! in_rect || gegl_rectangle_is_infinite_plane (in_rect))
    {
      return *roi;
    }

  return *in_rect;
}

static gboolean
operation_process (GeglOperation        *operation,
                   GeglOperationContext *context,
                   const gchar          *output_prop,
                   const GeglRectangle  *result,
                   gint                  level)
{
  GeglOperationClass  *operation_class;

  const GeglRectangle *in_rect =
    gegl_operation_source_get_bounding_box (operation, "input");

  if (in_rect && gegl_rectangle_is_infinite_plane (in_rect))
    {
      gpointer in = gegl_operation_context_get_object (context, "input");
      gegl_operation_context_take_object (context, "output",
                                          g_object_ref (G_OBJECT (in)));
      return TRUE;
    }

  operation_class = GEGL_OPERATION_CLASS (gegl_op_parent_class);

  return operation_class->process (operation, context, output_prop, result,
                                   gegl_operation_context_get_level (context));
}


static void
gegl_op_class_init (GeglOpClass *klass)
{
  GeglOperationClass       *operation_class;
  GeglOperationFilterClass *filter_class;

  operation_class          = GEGL_OPERATION_CLASS (klass);
  filter_class             = GEGL_OPERATION_FILTER_CLASS (klass);

  operation_class->get_required_for_output = get_required_for_output;
  operation_class->process                 = operation_process;

  filter_class->process = process;

  gegl_operation_class_set_keys (operation_class,
    "name",        "gegl:seamkiller",
    "title",       _("Seamkiller"),
    "categories",  "map",
    "description", _("Use periodic + smooth decomposition to remove tiling seams"),
    NULL);
}

#endif

