/* Known-answer probe for mayhem/test.sh: opens a font, then prints what FreeType computes for it
   (face metrics, charmap lookups, advances, outline extents, bitmap checksum) so the test can
   compare the output against fixed expected values. */
#include <stdio.h>
#include <stdlib.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_BBOX_H
#include FT_FONT_FORMATS_H


int
main( int    argc,
      char** argv )
{
  FT_Library  library;
  FT_Face     face;
  FT_UInt     gid;
  FT_ULong    ch;
  int         i;


  if ( argc < 2 || FT_Init_FreeType( &library ) )
    return 2;

  if ( FT_New_Face( library, argv[1], 0, &face ) )
    return 3;

  printf( "format=%s glyphs=%ld upem=%u faces=%ld charmaps=%d\n",
          FT_Get_X11_Font_Format( face ), (long)face->num_glyphs,
          (unsigned)face->units_per_EM, (long)face->num_faces,
          face->num_charmaps );

  if ( FT_IS_SCALABLE( face ) )
    FT_Set_Pixel_Sizes( face, 0, 64 );
  else
    FT_Select_Size( face, 0 );

  for ( i = 1; i < argc - 1; i++ )
  {
    if ( argv[i + 1][0] == 'g' )
    {
      ch  = 0;
      gid = (FT_UInt)strtoul( argv[i + 1] + 1, NULL, 0 );
    }
    else
    {
      ch  = (FT_ULong)strtoul( argv[i + 1], NULL, 0 );
      gid = FT_Get_Char_Index( face, ch );
    }
    printf( "char=%lu gid=%u", ch, gid );

    if ( FT_IS_SCALABLE( face ) &&
         !FT_Load_Glyph( face, gid, FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP ) )
    {
      FT_BBox  bbox;


      FT_Outline_Get_CBox( &face->glyph->outline, &bbox );
      printf( " adv=%ld points=%d contours=%d cbox=%ld,%ld,%ld,%ld",
              (long)face->glyph->advance.x, face->glyph->outline.n_points,
              face->glyph->outline.n_contours, (long)bbox.xMin,
              (long)bbox.yMin, (long)bbox.xMax, (long)bbox.yMax );
    }
    else if ( !FT_Load_Glyph( face, gid, FT_LOAD_RENDER ) )
    {
      unsigned long  sum = 0;
      unsigned       r, c;
      FT_Bitmap*     b = &face->glyph->bitmap;


      for ( r = 0; r < b->rows; r++ )
        for ( c = 0; c < b->pitch; c++ )
          sum = sum * 31 + b->buffer[r * b->pitch + c];
      printf( " adv=%ld bitmap=%ux%u sum=%lu", (long)face->glyph->advance.x,
              b->width, b->rows, sum );
    }
    printf( "\n" );
  }

  FT_Done_Face( face );
  FT_Done_FreeType( library );
  return 0;
}
