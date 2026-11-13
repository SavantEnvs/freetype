/* Table validator fuzzer: runs the OpenType (BASE/GDEF/GPOS/GSUB/JSTF), TrueType GX
   (feat/mort/morx/bsln/just/kern/opbd/trak/prop/lcar) and classic 'kern' validators on the input.
   The validators are optional modules (src/otvalid, src/gxvalid) that the default module list leaves
   out; mayhem/build.sh compiles them and this harness registers them on each fresh library. */
#include <stddef.h>
#include <stdint.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OPENTYPE_VALIDATE_H
#include FT_GX_VALIDATE_H
#include FT_MODULE_H

extern const FT_Module_Class  otv_module_class;
extern const FT_Module_Class  gxv_module_class;


int
LLVMFuzzerTestOneInput( const uint8_t*  data,
                        size_t          size )
{
  FT_Library  library;
  FT_Face     face;
  FT_Bytes    base = NULL, gdef = NULL, gpos = NULL, gsub = NULL, jstf = NULL;
  FT_Bytes    tables[FT_VALIDATE_GX_LENGTH] = { NULL };
  FT_Bytes    ckern = NULL;
  int         i;


  if ( FT_Init_FreeType( &library ) )
    return 0;

  if ( FT_Add_Module( library, &otv_module_class ) ||
       FT_Add_Module( library, &gxv_module_class ) )
  {
    FT_Done_FreeType( library );
    return 0;
  }

  if ( FT_New_Memory_Face( library, data, (FT_Long)size, 0, &face ) )
  {
    FT_Done_FreeType( library );
    return -1;
  }

  if ( !FT_OpenType_Validate( face, FT_VALIDATE_OT, &base, &gdef, &gpos, &gsub, &jstf ) )
    FT_OpenType_Free( face, base ), FT_OpenType_Free( face, gdef ),
    FT_OpenType_Free( face, gpos ), FT_OpenType_Free( face, gsub ),
    FT_OpenType_Free( face, jstf );

  if ( !FT_TrueTypeGX_Validate( face, FT_VALIDATE_GX, tables, FT_VALIDATE_GX_LENGTH ) )
    for ( i = 0; i < FT_VALIDATE_GX_LENGTH; i++ )
      FT_TrueTypeGX_Free( face, tables[i] );

  if ( !FT_ClassicKern_Validate( face, FT_VALIDATE_MS, &ckern ) )
    FT_ClassicKern_Free( face, ckern );
  ckern = NULL;
  if ( !FT_ClassicKern_Validate( face, FT_VALIDATE_APPLE, &ckern ) )
    FT_ClassicKern_Free( face, ckern );

  FT_Done_Face( face );
  FT_Done_FreeType( library );
  return 0;
}
