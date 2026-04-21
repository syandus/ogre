/*
-----------------------------------------------------------------------------
This source file is part of OGRE
(Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2014 Torus Knot Software Ltd

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-----------------------------------------------------------------------------
*/

#include "OgreStableHeaders.h"

#include "OgrePVRTCCodec.h"
#include "OgreImage.h"

#define PVR_TEXTURE_FLAG_TYPE_MASK  0xff

namespace Ogre {
    
#if OGRE_COMPILER == OGRE_COMPILER_MSVC
#pragma pack (push, 1)
#else
#pragma pack (1)
#endif

    const uint32 PVR2_MAGIC = FOURCC('P', 'V', 'R', '!'); 
    const uint32 PVR3_MAGIC = FOURCC('P', 'V', 'R', 3); 

    enum
    {
        kPVRTextureFlagTypePVRTC_2 = 24,
        kPVRTextureFlagTypePVRTC_4
    };

    // adapted from https://github.com/infiniteflight/PVRTexLibNET/blob/adf43979a44029cad24f0d6888337c41dee9458a/PVRTexLibWrapper/PVRTexTool/Library/Include/PVRTTexture.h
    // MIT License
    enum
    {
        kPVRTPF_PVRTCI_2bpp_RGB,
        kPVRTPF_PVRTCI_2bpp_RGBA,
        kPVRTPF_PVRTCI_4bpp_RGB,
        kPVRTPF_PVRTCI_4bpp_RGBA,
        kPVRTPF_PVRTCII_2bpp,
        kPVRTPF_PVRTCII_4bpp,
        kPVRTPF_ETC1,
        kPVRTPF_DXT1,
        kPVRTPF_DXT2,
        kPVRTPF_DXT3,
        kPVRTPF_DXT4,
        kPVRTPF_DXT5,

        //These formats are identical to some DXT formats.
        kPVRTPF_BC1 = kPVRTPF_DXT1,
        kPVRTPF_BC2 = kPVRTPF_DXT3,
        kPVRTPF_BC3 = kPVRTPF_DXT5,

        //These are currently unsupported:
        kPVRTPF_BC4,
        kPVRTPF_BC5,
        kPVRTPF_BC6,
        kPVRTPF_BC7,

        //These are supported
        kPVRTPF_UYVY,
        kPVRTPF_YUY2,
        kPVRTPF_BW1bpp,
        kPVRTPF_SharedExponentR9G9B9E5,
        kPVRTPF_RGBG8888,
        kPVRTPF_GRGB8888,
        kPVRTPF_ETC2_RGB,
        kPVRTPF_ETC2_RGBA,
        kPVRTPF_ETC2_RGB_A1,
        kPVRTPF_EAC_R11,
        kPVRTPF_EAC_RG11,

        kPVRTPF_ASTC_4x4,
        kPVRTPF_ASTC_5x4,
        kPVRTPF_ASTC_5x5,
        kPVRTPF_ASTC_6x5,
        kPVRTPF_ASTC_6x6,
        kPVRTPF_ASTC_8x5,
        kPVRTPF_ASTC_8x6,
        kPVRTPF_ASTC_8x8,
        kPVRTPF_ASTC_10x5,
        kPVRTPF_ASTC_10x6,
        kPVRTPF_ASTC_10x8,
        kPVRTPF_ASTC_10x10,
        kPVRTPF_ASTC_12x10,
        kPVRTPF_ASTC_12x12,

        kPVRTPF_ASTC_3x3x3,
        kPVRTPF_ASTC_4x3x3,
        kPVRTPF_ASTC_4x4x3,
        kPVRTPF_ASTC_4x4x4,
        kPVRTPF_ASTC_5x4x4,
        kPVRTPF_ASTC_5x5x4,
        kPVRTPF_ASTC_5x5x5,
        kPVRTPF_ASTC_6x5x5,
        kPVRTPF_ASTC_6x6x5,
        kPVRTPF_ASTC_6x6x6,

        //Invalid value
        kPVRTPF_NumCompressedPFs
    };

    typedef struct _PVRTCTexHeaderV2
    {
        uint32 headerLength;
        uint32 height;
        uint32 width;
        uint32 numMipmaps;
        uint32 flags;
        uint32 dataLength;
        uint32 bpp;
        uint32 bitmaskRed;
        uint32 bitmaskGreen;
        uint32 bitmaskBlue;
        uint32 bitmaskAlpha;
        uint32 pvrTag;
        uint32 numSurfs;
    } PVRTCTexHeaderV2;

    typedef struct _PVRTCTexHeaderV3
    {
        uint32  version;         //Version of the file header, used to identify it.
        uint32  flags;           //Various format flags.
        uint64  pixelFormat;     //The pixel format, 8cc value storing the 4 channel identifiers and their respective sizes.
        uint32  colourSpace;     //The Colour Space of the texture, currently either linear RGB or sRGB.
        uint32  channelType;     //Variable type that the channel is stored in. Supports signed/unsigned int/short/byte or float for now.
        uint32  height;          //Height of the texture.
        uint32  width;           //Width of the texture.
        uint32  depth;           //Depth of the texture. (Z-slices)
        uint32  numSurfaces;     //Number of members in a Texture Array.
        uint32  numFaces;        //Number of faces in a Cube Map. Maybe be a value other than 6.
        uint32  mipMapCount;     //Number of MIP Maps in the texture - NB: Includes top level.
        uint32  metaDataSize;    //Size of the accompanying meta data.
    } PVRTCTexHeaderV3;

    typedef struct _PVRTCMetaData
    {
        uint32 DevFOURCC;
        uint32 u32Key;
        uint32 u32DataSize;
        uint8* Data;
    } PVRTCMetadata;
    
#if OGRE_COMPILER == OGRE_COMPILER_MSVC
#pragma pack (pop)
#else
#pragma pack ()
#endif

    //---------------------------------------------------------------------
    PVRTCCodec* PVRTCCodec::msInstance = 0;
    //---------------------------------------------------------------------
    void PVRTCCodec::startup(void)
    {
        if (!msInstance)
        {
            LogManager::getSingleton().logMessage(
                LML_NORMAL,
                "PVRTC codec registering");

            msInstance = OGRE_NEW PVRTCCodec();
            Codec::registerCodec(msInstance);
        }
    }
    //---------------------------------------------------------------------
    void PVRTCCodec::shutdown(void)
    {
        if(msInstance)
        {
            Codec::unregisterCodec(msInstance);
            OGRE_DELETE msInstance;
            msInstance = 0;
        }
    }
    //---------------------------------------------------------------------
    PVRTCCodec::PVRTCCodec():
        mType("pvr")
    { 
    }
    //---------------------------------------------------------------------
    void PVRTCCodec::decode(const DataStreamPtr& stream, const Any& output) const
    {
        Image* image = any_cast<Image*>(output);

        // Assume its a pvr 2 header
        PVRTCTexHeaderV2 headerV2;
        stream->read(&headerV2, sizeof(PVRTCTexHeaderV2));
        stream->seek(0);

        if (PVR2_MAGIC == headerV2.pvrTag)
        {           
            decodeV2(stream, image);
            return;
        }

        // Try it as pvr 3 header
        PVRTCTexHeaderV3 headerV3;
        stream->read(&headerV3, sizeof(PVRTCTexHeaderV3));
        stream->seek(0);

        if (PVR3_MAGIC == headerV3.version)
        {
            decodeV3(stream, image);
            return;
        }

        
        OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS,
                        "This is not a PVR2 / PVR3 file!", "PVRTCCodec::decode");
    }
    //---------------------------------------------------------------------    
    void PVRTCCodec::decodeV2(const DataStreamPtr& stream, Image* image)
    {
        PVRTCTexHeaderV2 header;
        uint32 flags = 0, formatFlags = 0;

        // Read the PVRTC header
        stream->read(&header, sizeof(PVRTCTexHeaderV2));

        // Get format flags
        flags = header.flags;
        flipEndian(&flags, sizeof(uint32));
        formatFlags = flags & PVR_TEXTURE_FLAG_TYPE_MASK;

        uint32 bitmaskAlpha = header.bitmaskAlpha;
        flipEndian(&bitmaskAlpha, sizeof(uint32));

        PixelFormat format = PF_UNKNOWN;
        if (formatFlags == kPVRTextureFlagTypePVRTC_4 || formatFlags == kPVRTextureFlagTypePVRTC_2)
        {
            if (formatFlags == kPVRTextureFlagTypePVRTC_4)
            {
                format = bitmaskAlpha ? PF_PVRTC_RGBA4 : PF_PVRTC_RGB4;
            }
            else if (formatFlags == kPVRTextureFlagTypePVRTC_2)
            {
                format = bitmaskAlpha ? PF_PVRTC_RGBA2 : PF_PVRTC_RGB2;
            }
        }
        else
        {
            OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, "Invalid format");
        }

        // Calculate total size from number of mipmaps, faces and size
        image->create(format, header.width, header.height, 1, 1, header.numMipmaps);
        stream->read(image->getData(), image->getSize());
    }
    //---------------------------------------------------------------------    
    void PVRTCCodec::decodeV3(const DataStreamPtr& stream, Image* image)
    {
        PVRTCTexHeaderV3 header;
        uint32 flags = 0;

        // Read the PVRTC header
        stream->read(&header, sizeof(PVRTCTexHeaderV3));

        // Read the PVRTC metadata
        if(header.metaDataSize)
        {
            stream->skip(header.metaDataSize);
        }

        // Identify the pixel format
        PixelFormat format = PF_UNKNOWN;
        switch (header.pixelFormat)
        {
            case kPVRTPF_PVRTCI_2bpp_RGB:
                format = PF_PVRTC_RGB2;
                break;
            case kPVRTPF_PVRTCI_2bpp_RGBA:
                format = PF_PVRTC_RGBA2;
                break;
            case kPVRTPF_PVRTCI_4bpp_RGB:
                format = PF_PVRTC_RGB4;
                break;
            case kPVRTPF_PVRTCI_4bpp_RGBA:
                format = PF_PVRTC_RGBA4;
                break;
            case kPVRTPF_PVRTCII_2bpp:
                format = PF_PVRTC2_2BPP;
                break;
            case kPVRTPF_PVRTCII_4bpp:
                format = PF_PVRTC2_4BPP;
                break;
            case kPVRTPF_ETC1:
                format = PF_ETC1_RGB8;
                break;
            case kPVRTPF_DXT1:
                format = PF_DXT1;
                break;
            case kPVRTPF_DXT2:
                format = PF_DXT2;
                break;
            case kPVRTPF_DXT3:
                format = PF_DXT3;
                break;
            case kPVRTPF_DXT4:
                format = PF_DXT4;
                break;
            case kPVRTPF_DXT5:
                format = PF_DXT5;
                break;
            case kPVRTPF_BC4:
                format = PF_BC4_UNORM;
                break;
            case kPVRTPF_BC5:
                format = PF_BC5_UNORM;
                break;
            case kPVRTPF_BC6:
                format = PF_BC6H_UF16;
                break;
            case kPVRTPF_BC7:
                format = PF_BC7_UNORM;
                break;
            case kPVRTPF_ETC2_RGB:
                format = PF_ETC2_RGB8;
                break;
            case kPVRTPF_ETC2_RGBA:
                format = PF_ETC2_RGBA8;
                break;
            case kPVRTPF_ETC2_RGB_A1:
                format = PF_ETC2_RGB8A1;
                break;
            case kPVRTPF_ASTC_4x4:
                format = PF_ASTC_RGBA_4X4_LDR;
                break;
            case kPVRTPF_ASTC_5x4:
                format = PF_ASTC_RGBA_5X4_LDR;
                break;
            case kPVRTPF_ASTC_5x5:
                format = PF_ASTC_RGBA_5X5_LDR;
                break;
            case kPVRTPF_ASTC_6x5:
                format = PF_ASTC_RGBA_6X5_LDR;
                break;
            case kPVRTPF_ASTC_6x6:
                format = PF_ASTC_RGBA_6X6_LDR;
                break;
            case kPVRTPF_ASTC_8x5:
                format = PF_ASTC_RGBA_8X5_LDR;
                break;
            case kPVRTPF_ASTC_8x6:
                format = PF_ASTC_RGBA_8X6_LDR;
                break;
            case kPVRTPF_ASTC_8x8:
                format = PF_ASTC_RGBA_8X8_LDR;
                break;
            case kPVRTPF_ASTC_10x5:
                format = PF_ASTC_RGBA_10X5_LDR;
                break;
            case kPVRTPF_ASTC_10x6:
                format = PF_ASTC_RGBA_10X6_LDR;
                break;
            case kPVRTPF_ASTC_10x8:
                format = PF_ASTC_RGBA_10X8_LDR;
                break;
            case kPVRTPF_ASTC_10x10:
                format = PF_ASTC_RGBA_10X10_LDR;
                break;
            case kPVRTPF_ASTC_12x10:
                format = PF_ASTC_RGBA_12X10_LDR;
                break;
            case kPVRTPF_ASTC_12x12:
                format = PF_ASTC_RGBA_12X12_LDR;
                break;
            default:
                OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, "Invalid format: " + StringConverter::toString(header.pixelFormat));
        }

        // Get format flags
        flags = header.flags;
        flipEndian(&flags, sizeof(uint32));

        // PVR v3 counts the base level; Ogre stores only additional mip levels.
        uint32 numMipMaps = header.mipMapCount ? header.mipMapCount - 1 : 0;
        image->create(format, header.width, header.height, header.depth, header.numFaces, numMipMaps);

        // Now deal with the data
        void *destPtr = image->getData();
        
        uint width = image->getWidth();
        uint height = image->getHeight();
        uint depth = image->getDepth();

        // All mips for a surface, then each face
        for(size_t mip = 0; mip <= image->getNumMipmaps(); ++mip)
        {
            for(size_t surface = 0; surface < header.numSurfaces; ++surface)
            {
                for(size_t i = 0; i < image->getNumFaces(); ++i)
                {
                    // Load directly
                    size_t pvrSize = PixelUtil::getMemorySize(width, height, depth, format);
                    stream->read(destPtr, pvrSize);
                    destPtr = static_cast<void*>(static_cast<uchar*>(destPtr) + pvrSize);
                }
            }

            // Next mip
            if(width!=1) width /= 2;
            if(height!=1) height /= 2;
            if(depth!=1) depth /= 2;
        }
    }
    //---------------------------------------------------------------------    
    String PVRTCCodec::getType() const 
    {
        return mType;
    }
    //---------------------------------------------------------------------
    String PVRTCCodec::magicNumberToFileExt(const char *magicNumberPtr, size_t maxbytes) const
    {
        if (maxbytes >= sizeof(uint32))
        {
            uint32 fileType;
            memcpy(&fileType, magicNumberPtr, sizeof(uint32));
			flipEndian(&fileType, sizeof(uint32));

            if (PVR3_MAGIC == fileType || PVR2_MAGIC == fileType)
            {
                return String("pvr");
            }
        }

        return BLANKSTRING;
    }
}
