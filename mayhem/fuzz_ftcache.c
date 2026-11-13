/* Cache sub-system fuzzer: serves the input font through FTC_Manager and queries the image, small
   bitmap and charmap caches, resetting the manager between rounds. */
#include <stddef.h>
#include <stdint.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_CACHE_H

#define MAX_GLYPHS  24

typedef struct  Blob_
{
  const FT_Byte*  data;
  FT_Long         size;

} Blob;


static FT_Error
requester( FTC_FaceID   face_id,
           FT_Library   library,
           FT_Pointer   req_data,
           FT_Face*     aface )
{
  Blob*  blob = (Blob*)req_data;


  return FT_New_Memory_Face( library,
                             blob->data,
                             blob->size,
                             (FT_Long)(intptr_t)face_id,
                             aface );
}


int
LLVMFuzzerTestOneInput( const uint8_t*  data,
                        size_t          size )
{
  FT_Library     library;
  FTC_Manager    manager;
  FTC_ImageCache image_cache;
  FTC_SBitCache  sbit_cache;
  FTC_CMapCache  cmap_cache;
  Blob           blob;
  FT_Face        face;
  FT_UInt        round, i;
  int            rejected = 0;


  blob.data = data;
  blob.size = (FT_Long)size;

  if ( FT_Init_FreeType( &library ) )
    return 0;

  if ( FT_New_Memory_Face( library, data, blob.size, 0, &face ) )
  {
    FT_Done_FreeType( library );
    return -1;
  }
  FT_Done_Face( face );

  if ( FTC_Manager_New( library, 2, 2, 256 * 1024, requester, &blob, &manager ) )
    goto Exit;

  if ( FTC_ImageCache_New( manager, &image_cache ) ||
       FTC_SBitCache_New( manager, &sbit_cache )   ||
       FTC_CMapCache_New( manager, &cmap_cache )   )
    goto Done;

  for ( round = 0; round < 3; round++ )
  {
    FTC_ImageTypeRec  type;
    FT_Face           f;
    FT_Size           s;
    FTC_ScalerRec     scaler;


    type.face_id    = (FTC_FaceID)(intptr_t)( round & 1 );
    type.width      = 12 + 4 * round;
    type.height     = 12 + 4 * round;
    type.flags      = round == 2 ? FT_LOAD_NO_HINTING : FT_LOAD_DEFAULT;

    scaler.face_id  = type.face_id;
    scaler.width    = type.width;
    scaler.height   = type.height;
    scaler.pixel    = 1;
    scaler.x_res    = 0;
    scaler.y_res    = 0;

    FTC_Manager_LookupFace( manager, type.face_id, &f );
    FTC_Manager_LookupSize( manager, &scaler, &s );

    for ( i = 0; i < MAX_GLYPHS; i++ )
    {
      FT_Glyph        glyph;
      FTC_SBit        sbit;
      FTC_Node        node = NULL;
      FT_UInt         gindex = i * ( round + 1 );


      if ( !FTC_ImageCache_Lookup( image_cache, &type, gindex, &glyph, &node ) )
        FTC_Node_Unref( node, manager );

      node = NULL;
      if ( !FTC_SBitCache_Lookup( sbit_cache, &type, gindex, &sbit, &node ) )
        FTC_Node_Unref( node, manager );

      FTC_CMapCache_Lookup( cmap_cache, type.face_id, -1, 0x41 + i * 37 );
      FTC_CMapCache_Lookup( cmap_cache, type.face_id, 0, 0x4E00 + i );
    }

    if ( round == 1 )
      FTC_Manager_RemoveFaceID( manager, type.face_id );
    else if ( round == 2 )
      FTC_Manager_Reset( manager );
  }

Done:
  FTC_Manager_Done( manager );
Exit:
  FT_Done_FreeType( library );
  return rejected ? -1 : 0;
}
