/* Variable / multiple-master font fuzzer: opens named instances, sets design and blend coordinates
   at the axis extremes through the public MM/variation API, and loads glyphs at each setting. */
#include <stddef.h>
#include <stdint.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H
#include FT_OUTLINE_H
#include FT_SIZES_H

#define MAX_INSTANCES  4
#define MAX_GLYPHS     8
#define MAX_AXES       64


static void
load_some_glyphs( FT_Face  face )
{
  FT_Long  n = face->num_glyphs < MAX_GLYPHS ? face->num_glyphs : MAX_GLYPHS;
  FT_Long  i;


  for ( i = 0; i < n; i++ )
  {
    FT_Int32  flags = ( i & 1 ) ? FT_LOAD_NO_HINTING : FT_LOAD_DEFAULT;


    if ( !FT_Load_Glyph( face, (FT_UInt)i, flags | FT_LOAD_NO_BITMAP ) &&
         face->glyph->format == FT_GLYPH_FORMAT_OUTLINE )
      FT_Render_Glyph( face->glyph, FT_RENDER_MODE_NORMAL );
  }

  FT_Load_Glyph( face, 0, FT_LOAD_NO_SCALE );
}


static void
vary_face( FT_Face  face )
{
  FT_MM_Var*  mm = NULL;
  FT_Fixed    coords[MAX_AXES];
  FT_Fixed    got[MAX_AXES];
  FT_UInt     num_axes, i, round;


  if ( FT_Get_MM_Var( face, &mm ) )
    return;

  num_axes = mm->num_axis < MAX_AXES ? mm->num_axis : MAX_AXES;

  for ( round = 0; round < 4; round++ )
  {
    for ( i = 0; i < num_axes; i++ )
    {
      switch ( round )
      {
      case 0:  coords[i] = mm->axis[i].minimum; break;
      case 1:  coords[i] = mm->axis[i].maximum; break;
      case 2:  coords[i] = mm->axis[i].def;     break;
      default: coords[i] = ( i & 1 ) ? mm->axis[i].maximum
                                     : mm->axis[i].minimum;
      }
    }

    if ( !FT_Set_Var_Design_Coordinates( face, num_axes, coords ) )
    {
      FT_Get_Var_Design_Coordinates( face, num_axes, got );
      FT_Get_Var_Blend_Coordinates( face, num_axes, got );
      load_some_glyphs( face );
    }
  }

  for ( i = 0; i < num_axes; i++ )
    coords[i] = ( i & 1 ) ? 0x10000 : -0x10000;
  if ( !FT_Set_Var_Blend_Coordinates( face, num_axes, coords ) )
    load_some_glyphs( face );

  for ( i = 0; i < num_axes; i++ )
    coords[i] = 0x8000;
  if ( !FT_Set_MM_Blend_Coordinates( face, num_axes, coords ) )
    load_some_glyphs( face );
  if ( !FT_Set_MM_Design_Coordinates( face, num_axes, coords ) )
    load_some_glyphs( face );

  for ( i = 0; i < mm->num_namedstyles && i < MAX_INSTANCES; i++ )
    if ( !FT_Set_Named_Instance( face, i + 1 ) )
      load_some_glyphs( face );

  FT_Set_Var_Design_Coordinates( face, 0, NULL );
  FT_Set_Var_Blend_Coordinates( face, 0, NULL );
  FT_Done_MM_Var( face->glyph->library, mm );
}


int
LLVMFuzzerTestOneInput( const uint8_t*  data,
                        size_t          size )
{
  FT_Library  library;
  FT_Face     face;
  FT_Long     num_instances, inst;


  if ( FT_Init_FreeType( &library ) )
    return 0;

  if ( FT_New_Memory_Face( library, data, (FT_Long)size, 0, &face ) )
  {
    FT_Done_FreeType( library );
    return -1;
  }

  num_instances = face->style_flags >> 16;
  if ( num_instances > MAX_INSTANCES )
    num_instances = MAX_INSTANCES;

  if ( FT_IS_SCALABLE( face ) )
    FT_Set_Char_Size( face, 0, 14 * 64, 72, 72 );
  else
    FT_Set_Pixel_Sizes( face, 0, 14 );

  vary_face( face );
  FT_Done_Face( face );

  for ( inst = 1; inst <= num_instances; inst++ )
  {
    if ( FT_New_Memory_Face( library,
                             data,
                             (FT_Long)size,
                             ( inst << 16 ),
                             &face ) )
      continue;

    if ( FT_IS_SCALABLE( face ) )
      FT_Set_Char_Size( face, 0, 14 * 64, 72, 72 );

    load_some_glyphs( face );
    vary_face( face );
    FT_Done_Face( face );
  }

  FT_Done_FreeType( library );
  return 0;
}
