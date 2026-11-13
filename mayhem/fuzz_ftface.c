/* Generic font fuzzer: opens the input as a font through every built-in driver (TrueType, CFF,
   Type 1, Type 42, CID, PFR, BDF, PCF, FNT/FON, SFNT-wrapped bitmaps, WOFF/WOFF2, gzip/bzip2/LZW
   compressed streams), then loads and renders glyphs with several hinting modes. */
#include <stddef.h>
#include <stdint.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
#include FT_BBOX_H
#include FT_SIZES_H
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_TABLES_H
#include FT_TRUETYPE_TAGS_H
#include FT_BDF_H
#include FT_WINFONTS_H
#include FT_TYPE1_TABLES_H
#include FT_FONT_FORMATS_H
#include FT_ADVANCES_H

#define MAX_SAMPLED_GLYPHS  16
#define MAX_CHARMAP_CHARS   64

static const FT_Int32  load_flags[] = {
  FT_LOAD_DEFAULT,
  FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP,
  FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_LIGHT,
  FT_LOAD_NO_AUTOHINT | FT_LOAD_TARGET_MONO,
  FT_LOAD_NO_SCALE | FT_LOAD_NO_RECURSE,
  FT_LOAD_COLOR | FT_LOAD_TARGET_LCD,
  FT_LOAD_NO_BITMAP | FT_LOAD_PEDANTIC,
};


static void
walk_charmaps( FT_Face  face )
{
  int  i;


  for ( i = 0; i < face->num_charmaps && i < 8; i++ )
  {
    FT_UInt   gindex;
    FT_ULong  charcode;
    int       n = 0;


    if ( FT_Set_Charmap( face, face->charmaps[i] ) )
      continue;

    FT_Get_Char_Index( face, 'A' );
    FT_Get_Char_Index( face, 0x20AC );
    FT_Face_GetCharVariantIndex( face, 'A', 0xFE00 );

    charcode = FT_Get_First_Char( face, &gindex );
    while ( gindex != 0 && n++ < MAX_CHARMAP_CHARS )
      charcode = FT_Get_Next_Char( face, charcode, &gindex );
  }
}


static void
query_format_tables( FT_Face  face )
{
  FT_UInt              i, count;
  FT_SfntName          name;
  FT_Int               idx;
  FT_WinFNT_HeaderRec  winfnt;
  PS_FontInfoRec       psinfo;
  PS_PrivateRec        pspriv;
  BDF_PropertyRec      prop;
  const char*          bdf_name;
  const char*          bdf_charset[2];
  FT_ULong             length;
  FT_Byte              buf[64];


  FT_Get_Postscript_Name( face );
  FT_Get_X11_Font_Format( face );
  FT_Get_FSType_Flags( face );
  FT_Has_PS_Glyph_Names( face );
  FT_Get_Sfnt_Table( face, FT_SFNT_HEAD );
  FT_Get_Sfnt_Table( face, FT_SFNT_MAXP );
  FT_Get_Sfnt_Table( face, FT_SFNT_OS2 );
  FT_Get_Sfnt_Table( face, FT_SFNT_POST );
  FT_Get_Sfnt_Table( face, FT_SFNT_HHEA );
  FT_Get_Sfnt_Table( face, FT_SFNT_VHEA );
  FT_Get_PS_Font_Info( face, &psinfo );
  FT_Get_PS_Font_Private( face, &pspriv );
  FT_Get_WinFNT_Header( face, &winfnt );

  count = FT_Get_Sfnt_Name_Count( face );
  for ( i = 0; i < count && i < 16; i++ )
    FT_Get_Sfnt_Name( face, i, &name );

  length = 0;
  if ( !FT_Load_Sfnt_Table( face, 0, 0, NULL, &length ) )
  {
    length = sizeof ( buf );
    FT_Load_Sfnt_Table( face, 0, 0, buf, &length );
  }
  FT_Sfnt_Table_Info( face, 0, NULL, &length );

  bdf_charset[0] = bdf_charset[1] = NULL;
  FT_Get_BDF_Charset_ID( face, &bdf_charset[0], &bdf_charset[1] );
  bdf_name = "FONT_ASCENT";
  FT_Get_BDF_Property( face, bdf_name, &prop );

  for ( idx = 0; idx < 4; idx++ )
    FT_Get_Name_Index( face, "A" );
}


static void
exercise_glyph( FT_Face   face,
                FT_UInt   gindex,
                FT_Int32  flags )
{
  FT_GlyphSlot  slot = face->glyph;
  FT_Glyph      glyph;
  FT_BBox       bbox;
  FT_Fixed      advance;
  char          gname[64];


  if ( FT_Load_Glyph( face, gindex, flags ) )
    return;

  FT_Get_Glyph_Name( face, gindex, gname, sizeof ( gname ) );
  FT_Get_Advance( face, gindex, flags, &advance );

  if ( slot->format == FT_GLYPH_FORMAT_OUTLINE )
  {
    FT_Outline_Get_CBox( &slot->outline, &bbox );
    FT_Outline_Get_Orientation( &slot->outline );
  }

  if ( !FT_Get_Glyph( slot, &glyph ) )
  {
    FT_Glyph_Get_CBox( glyph, FT_GLYPH_BBOX_PIXELS, &bbox );
    FT_Done_Glyph( glyph );
  }

  if ( flags & FT_LOAD_NO_SCALE )
    return;

  if ( !FT_Render_Glyph( slot, FT_RENDER_MODE_NORMAL ) )
    FT_Render_Glyph( slot, FT_RENDER_MODE_LCD );

  if ( !FT_Load_Glyph( face, gindex, flags ) )
    FT_Render_Glyph( slot, FT_RENDER_MODE_SDF );
}


int
LLVMFuzzerTestOneInput( const uint8_t*  data,
                        size_t          size )
{
  FT_Library  library;
  FT_Face     face;
  FT_UInt     i, k, n;
  FT_Long     num_glyphs;


  if ( FT_Init_FreeType( &library ) )
    return 0;

  if ( FT_New_Memory_Face( library, data, (FT_Long)size, 0, &face ) )
  {
    FT_Done_FreeType( library );
    return -1;
  }

  query_format_tables( face );
  walk_charmaps( face );

  if ( FT_IS_SCALABLE( face ) )
    FT_Set_Char_Size( face, 0, 12 * 64, 96, 96 );
  else if ( face->num_fixed_sizes > 0 )
    FT_Select_Size( face, 0 );
  else
    FT_Set_Pixel_Sizes( face, 0, 16 );

  num_glyphs = face->num_glyphs;
  n          = num_glyphs < MAX_SAMPLED_GLYPHS ? (FT_UInt)num_glyphs
                                               : MAX_SAMPLED_GLYPHS;

  for ( k = 0; k < sizeof ( load_flags ) / sizeof ( load_flags[0] ); k++ )
    for ( i = 0; i < n; i++ )
    {
      FT_UInt  gindex = (FT_UInt)( (FT_Long)i * num_glyphs / (FT_Long)n );


      exercise_glyph( face, gindex, load_flags[k] );
    }

  if ( FT_HAS_KERNING( face ) && n >= 2 )
  {
    FT_Vector  kern;


    FT_Get_Kerning( face, 1, 2, FT_KERNING_DEFAULT, &kern );
    FT_Get_Kerning( face, 2, 1, FT_KERNING_UNSCALED, &kern );
  }

  if ( face->num_fixed_sizes > 1 )
  {
    FT_Select_Size( face, face->num_fixed_sizes - 1 );
    exercise_glyph( face, 1 % ( num_glyphs ? num_glyphs : 1 ), FT_LOAD_DEFAULT );
  }

  FT_Done_Face( face );
  FT_Done_FreeType( library );
  return 0;
}
