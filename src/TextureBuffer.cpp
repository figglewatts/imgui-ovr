#include "TextureBuffer.h"

TextureBuffer::TextureBuffer(ovrSession session, bool rendertarget, bool displayableOnHmd, ovrSizei size, int mipLevels, unsigned char * data, int sampleCount)
	:
	session(session),
	textureChain(nullptr),
	texId(0),
	fboId(0)
{
	texSize = size;

	if (displayableOnHmd)
	{
		// This texture isn't necessarily going to be a rendertarget, but it usually is.
		ovrTextureSwapChainDesc desc = {};
		desc.Type = ovrTexture_2D;
		desc.ArraySize = 1;
		desc.Width = size.w;
		desc.Height = size.h;
		desc.MipLevels = 1;
		desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.SampleCount = 1;
		desc.StaticImage = ovrFalse;

		ovrErrorInfo info;
		ovr_GetLastErrorInfo(&info);

		ovrResult result = ovr_CreateTextureSwapChainGL(session, &desc, &textureChain);

		int length = 0;
		ovr_GetTextureSwapChainLength(session, textureChain, &length);

		if (OVR_SUCCESS(result))
		{
			for (int i = 0; i < length; ++i)
			{
				GLuint chainTexId;
				ovr_GetTextureSwapChainBufferGL(session, textureChain, i, &chainTexId);
				glBindTexture(GL_TEXTURE_2D, chainTexId);

				if (rendertarget)
				{
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				}
				else
				{
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
				}
			}
		}
	}
	else
	{
		glGenTextures(1, &texId);
		glBindTexture(GL_TEXTURE_2D, texId);

		if (rendertarget)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
		else
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		}

		glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, texSize.w, texSize.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	}

	if (mipLevels > 1)
	{
		glGenerateMipmap(GL_TEXTURE_2D);
	}

	glGenFramebuffers(1, &fboId);
}
