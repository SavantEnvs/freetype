/* Outline and bitmap post-processing fuzzer: loads outline glyphs from the input font, then runs
   them through the stroker, the outline transforms/emboldening, and the bitmap helpers. */
#include <stddef.h>
#include <stdint.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
#include FT_STROKER_H
#include FT_BITMAP_H
#include FT_BBOX_H

#define MAX_GLYPHS  8


static int  nop_move( const FT_Vector* to, void* u )
{ (void)to; (void)u; return 0; }
static int  nop_line( const FT_Vector* to, void* u )
{ (void)to; (void)u; return 0; }
static int  nop_conic( const FT_Vector* c, const FT_Vector* to, void* u )
{ (void)c; (void)to; (void)u; return 0; }
static int  nop_cubic( const FT_Vector* c1, const FT_Vector* c2, const FT_Vector* to, void* u )
{ (void)c1; (void)c2; (void)to; (void)u; return 0; }


static void
stroke_glyph( FT_Library  library,
              FT_Face     face )
{
  static const FT_Stroker_LineCap   caps[]  = { FT_STROKER_LINECAP_BUTT,
                                                FT_STROKER_LINECAP_ROUND,
                                                FT_STROKER_LINECAP_SQUARE };
  static const FT_Stroker_LineJoin  joins[] = { FT_STROKER_LINEJOIN_ROUND,
                                                FT_STROKER_LINEJOIN_BEVEL,
                                                FT_STROKER_LINEJOIN_MITER,
                                                FT_STROKER_LINEJOIN_MITER_FIXED };
  FT_Stroker  stroker;
  FT_Glyph    glyph;
  unsigned    k;


  if ( FT_Stroker_New( library, &stroker ) )
    return;

  for ( k = 0; k < 4; k++ )
  {
    FT_Stroker_Set( stroker, 48 + 32 * k, caps[k % 3], joins[k], 0x20000 );

    if ( !FT_Get_Glyph( face->glyph, &glyph ) )
    {
      FT_Error  err = ( k & 1 ) ? FT_Glyph_Stroke( &glyph, stroker, 1 )
                                : FT_Glyph_StrokeBorder( &glyph, stroker,
                                                         k & 2, 1 );


      if ( !err )
      {
        FT_BBox  bbox;


        FT_Glyph_Get_CBox( glyph, FT_GLYPH_BBOX_SUBPIXELS, &bbox );
        if ( !FT_Glyph_To_Bitmap( &glyph, FT_RENDER_MODE_NORMAL, NULL, 1 ) )
          ;
        FT_Done_Glyph( glyph );
      }
    }
  }

  if ( !FT_Stroker_ParseOutline( stroker, &face->glyph->outline, 0 ) )
  {
    FT_UInt  npoints, ncontours;


    if ( !FT_Stroker_GetCounts( stroker, &npoints, &ncontours ) &&
         npoints < 100000 && ncontours < 10000 )
    {
      FT_Outline  out;


      if ( !FT_Outline_New( library, npoints, (FT_Int)ncontours, &out ) )
      {
        out.n_points   = 0;
        out.n_contours = 0;
        FT_Stroker_Export( stroker, &out );
        FT_Outline_Done( library, &out );
      }
    }
  }

  FT_Stroker_Done( stroker );
}


static void
transform_outline( FT_Library  library,
                   FT_Face     face )
{
  static const FT_Outline_Funcs  funcs = { nop_move, nop_line, nop_conic, nop_cubic, 0, 0 };
  FT_Outline  copy;
  FT_Matrix   m = { 0x12000, 0x3000, -0x2000, 0xF000 };
  FT_BBox     bbox;


  FT_Outline_Check( &face->glyph->outline );
  FT_Outline_Decompose( &face->glyph->outline, &funcs, NULL );

  if ( FT_Outline_New( library,
                       face->glyph->outline.n_points,
                       face->glyph->outline.n_contours,
                       &copy ) )
    return;

  if ( !FT_Outline_Copy( &face->glyph->outline, &copy ) )
  {
    FT_Outline_Embolden( &copy, 64 );
    FT_Outline_EmboldenXY( &copy, 32, -16 );
    FT_Outline_Transform( &copy, &m );
    FT_Outline_Translate( &copy, 100, -100 );
    FT_Outline_Reverse( &copy );
    FT_Outline_Get_BBox( &copy, &bbox );
    FT_Outline_Get_Orientation( &copy );
    FT_Outline_Check( &copy );
  }

  FT_Outline_Done( library, &copy );
}


static void
bitmap_ops( FT_Library  library,
            FT_Face     face )
{
  FT_Bitmap  target;
  FT_Int     align;


  FT_Bitmap_Init( &target );
  for ( align = 1; align <= 4; align *= 4 )
  {
    if ( !FT_Bitmap_Convert( library, &face->glyph->bitmap, &target, align ) )
      FT_Bitmap_Embolden( library, &target, 64, 64 );
  }
  FT_Bitmap_Done( library, &target );

  {
    FT_Bitmap  bgra;
    FT_Vector  src_off, dst_off;
    FT_Color   color = { 0x20, 0x40, 0x80, 0xFF };


    src_off.x = face->glyph->bitmap_left * 64;
    src_off.y = face->glyph->bitmap_top * 64;
    dst_off.x = dst_off.y = 0;

    FT_Bitmap_Init( &bgra );
    FT_Bitmap_Blend( library, &face->glyph->bitmap, src_off, &bgra, &dst_off, color );
    FT_Bitmap_Done( library, &bgra );
  }
}


int
LLVMFuzzerTestOneInput( const uint8_t*  data,
                        size_t          size )
{
  FT_Library  library;
  FT_Face     face;
  FT_Long     n, i;


  if ( FT_Init_FreeType( &library ) )
    return 0;

  if ( FT_New_Memory_Face( library, data, (FT_Long)size, 0, &face ) )
  {
    FT_Done_FreeType( library );
    return -1;
  }

  if ( FT_IS_SCALABLE( face ) )
    FT_Set_Char_Size( face, 0, 16 * 64, 96, 96 );
  else if ( face->num_fixed_sizes > 0 )
    FT_Select_Size( face, 0 );
  else
    FT_Set_Pixel_Sizes( face, 0, 16 );

  n = face->num_glyphs < MAX_GLYPHS ? face->num_glyphs : MAX_GLYPHS;
  for ( i = 0; i < n; i++ )
  {
    if ( FT_Load_Glyph( face, (FT_UInt)i, FT_LOAD_NO_HINTING ) )
      continue;

    if ( face->glyph->format == FT_GLYPH_FORMAT_OUTLINE )
    {
      stroke_glyph( library, face );
      transform_outline( library, face );

      if ( !FT_Render_Glyph( face->glyph, FT_RENDER_MODE_NORMAL ) )
        bitmap_ops( library, face );
    }
    else if ( face->glyph->format == FT_GLYPH_FORMAT_BITMAP )
      bitmap_ops( library, face );
  }

  FT_Done_Face( face );
  FT_Done_FreeType( library );
  return 0;
}
