//CC0 1.0 Universal https://creativecommons.org/publicdomain/zero/1.0/
//To the extent possible under law, Blackle Mori has waived all copyright and related or neighboring rights to this work.

/* This plugin performs a periodic + smooth decomposition on 
 * the input to remove the seams that appear when tiling.
 */

#define GEGL_ITERATOR2_API
#define GETTEXT_PACKAGE "gtk20"
#include <glib/gi18n-lib.h>

#ifdef GEGL_PROPERTIES

/* This operation has no properties */

#else

#define GEGL_OP_AREA_FILTER
#define GEGL_OP_NAME     seamkiller
#define GEGL_OP_C_SOURCE seamkiller.c

#include "gegl-op.h"

static void
prepare (GeglOperation *operation)
{
  const Babl *input_format = gegl_operation_get_source_format (operation, "input");
  const Babl *format;

  GeglOperationAreaFilter *op_area = GEGL_OPERATION_AREA_FILTER (operation);

  if (input_format == NULL || babl_format_has_alpha (input_format))
    format = babl_format_with_space ("R'G'B'A float", input_format);
  else
    format = babl_format_with_space ("R'G'B' float", input_format);

  gegl_operation_set_format (operation, "input", format);
  gegl_operation_set_format (operation, "output", format);

  op_area->left   =
  op_area->right  =
  op_area->top    =
  op_area->bottom = 1;
}

static GeglRectangle
get_bounding_box (GeglOperation *operation)
{
  GeglRectangle *region;

  region = gegl_operation_source_get_bounding_box (operation, "input");

  if (region != NULL)
    return *region;
  else
    return *GEGL_RECTANGLE (0, 0, 0, 0);
}

static gboolean
process (GeglOperation       *operation,
         GeglBuffer          *input,
         GeglBuffer          *output,
         const GeglRectangle *roi,
         gint                 level)
{
  /*
  const Babl   *format = gegl_operation_get_format (operation, "input");

  GeglSampler        *sampler;
  GeglBufferIterator *iter;
  gint                x, y;

  sampler = gegl_buffer_sampler_new_at_level (input, format,
                                              GEGL_SAMPLER_CUBIC, level);

  iter = gegl_buffer_iterator_new (output, roi, level, format,
                                   GEGL_ACCESS_WRITE, GEGL_ABYSS_NONE, 2);

  gegl_buffer_iterator_add (iter, input, roi, level, format,
                            GEGL_ACCESS_READ, GEGL_ABYSS_NONE);

  while (gegl_buffer_iterator_next (iter))
    {
      gfloat *out_pixel = iter->items[0].data;
      gfloat *in_pixel  = iter->items[1].data;

      for (y = iter->items[0].roi.y; y < iter->items[0].roi.y + iter->items[0].roi.height; y++)
        {

          for (x = iter->items[0].roi.x; x < iter->items[0].roi.x + iter->items[0].roi.width; x++)
            {
              gegl_sampler_get (sampler, x, y, NULL, out_pixel, GEGL_ABYSS_NONE);

              out_pixel += 4;
              in_pixel  += 4;
            }
        }
    }

  g_object_unref (sampler);

*/
  return  TRUE;
}


static void
gegl_op_class_init (GeglOpClass *klass)
{
  GeglOperationClass       *operation_class;
  GeglOperationFilterClass *filter_class;
  gchar                    *composition =
    "<?xml version='1.0' encoding='UTF-8'?>"
    "<gegl>"
    "  <node operation='gegl:crop' width='200' height='200'/>"
    "  <node operation='gegl:over'>"
    "    <node operation='gegl:seamkiller'/>"
    "    <node operation='gegl:threshold'/>"
    "    <node operation='gegl:load' path='standard-input.png'/>"
    "  </node>"
    "  <node operation='gegl:checkerboard'>"
    "    <params>"
    "      <param name='color1'>rgb(0.25,0.25,0.25)</param>"
    "      <param name='color2'>rgb(0.75,0.75,0.75)</param>"
    "    </params>"
    "  </node>"    
    "</gegl>";

  operation_class          = GEGL_OPERATION_CLASS (klass);
  filter_class             = GEGL_OPERATION_FILTER_CLASS (klass);

  operation_class->prepare          = prepare;
  operation_class->get_bounding_box = get_bounding_box;
  filter_class->process             = process;

  gegl_operation_class_set_keys (operation_class,
    "name",        "gegl:seamkiller",
    "title",       _("Seamkiller"),
    "categories",  "map",
    "license",     "CC0",
    "reference-composition", composition,
    "description", _("Use periodic + smooth decomposition to remove tiling seams"),
    NULL);
}

#endif

