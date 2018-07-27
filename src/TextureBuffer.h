/**
* @brief From Oculus SDK examples.
*
* @file TextureBuffer.h
* @author Oculus VR LLC.
*/

#pragma once
#include <LibOVR/OVR_CAPI_GL.h>
#include "GL.h"
#include "DepthBuffer.h"

struct TextureBuffer
{
	ovrSession          session;
	ovrTextureSwapChain  textureChain;
	GLuint              texId;
	GLuint              fboId;
	ovrSizei               texSize;

	TextureBuffer(ovrSession session, bool rendertarget, bool displayableOnHmd, ovrSizei size, int mipLevels, unsigned char * data, int sampleCount);

	~TextureBuffer()
	{
		if (textureChain)
		{
			ovr_DestroyTextureSwapChain(session, textureChain);
			textureChain = nullptr;
		}
		if (texId)
		{
			glDeleteTextures(1, &texId);
			texId = 0;
		}
		if (fboId)
		{
			glDeleteFramebuffers(1, &fboId);
			fboId = 0;
		}
	}

	ovrSizei GetSize() const
	{
		return texSize;
	}

	void SetAndClearRenderSurface(DepthBuffer* dbuffer)
	{
		GLuint curTexId;
		if (textureChain)
		{
			int curIndex;
			ovr_GetTextureSwapChainCurrentIndex(session, textureChain, &curIndex);
			ovr_GetTextureSwapChainBufferGL(session, textureChain, curIndex, &curTexId);
		}
		else
		{
			curTexId = texId;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, fboId);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, curTexId, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, dbuffer->texId, 0);

		glViewport(0, 0, texSize.w, texSize.h);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_FRAMEBUFFER_SRGB);
	}

	void UnsetRenderSurface()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, fboId);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
	}

	void Commit()
	{
		if (textureChain)
		{
			ovr_CommitTextureSwapChain(session, textureChain);
		}
	}
};
