/*
 * LegacyClonk
 *
 * Copyright (c) RedWolf Design
 * Copyright (c) 2001, Sven2
 * Copyright (c) 2017-2019, The LegacyClonk Team and contributors
 *
 * Distributed under the terms of the ISC license; see accompanying file
 * "COPYING" for details.
 *
 * "Clonk" is a registered trademark of Matthes Bender, used with permission.
 * See accompanying file "TRADEMARK" for details.
 *
 * To redistribute this file separately, substitute the full license texts
 * for the above references.
 */

/* OpenGL implementation of NewGfx */

#include <Standard.h>
#include <StdGL.h>
#include "C4Config.h"
#include <C4Surface.h>
#include <C4Log.h>
#include <StdWindow.h>

#ifndef USE_CONSOLE

#include <stdio.h>
#include <math.h>
#include <limits.h>

void CStdGLShader::Compile()
{
	if (shader) // recompiling?
	{
		glDeleteShader(shader);
	}

	GLenum t;
	switch (type)
	{
	case Type::Vertex:
		t = GL_VERTEX_SHADER;
		break;

	case Type::TesselationControl:
		t = GL_TESS_CONTROL_SHADER;
		break;

	case Type::TesselationEvaluation:
		t = GL_TESS_EVALUATION_SHADER;
		break;

	case Type::Geometry:
		t = GL_GEOMETRY_SHADER;
		break;

	case Type::Fragment:
		t = GL_FRAGMENT_SHADER;
		break;

	default:
		throw Exception{"Invalid shader type"};
	}

	shader = glCreateShader(t);
	if (!shader)
	{
		throw Exception{"Could not create shader"};
	}

	PrepareSource();

	GLint status = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (!status)
	{
		GLint size = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &size);
		if (size)
		{
			std::string errorMessage;
			errorMessage.resize(size);
			glGetShaderInfoLog(shader, size, NULL, errorMessage.data());
			throw Exception{errorMessage};
		}

		throw Exception{"Compile failed"};
	}
}

void CStdGLShader::Clear()
{
	if (shader)
	{
		glDeleteShader(shader);
		shader = 0;
	}

	CStdShader::Clear();
}

void CStdGLShader::PrepareSource()
{
	size_t pos = source.find("#version");
	if (pos == std::string::npos)
	{
		glDeleteShader(shader);
		throw Exception{"Version directive must be first statement and may not be repeated"};
	}

	pos = source.find('\n', pos + 1);
	assert(pos != std::string::npos);

	std::string copy = source;
	std::string buffer = "";

	for (const auto &[key, value] : macros)
	{
		buffer.append("#define ");
		buffer.append(key);
		buffer.append(" ");
		buffer.append(value);
		buffer.append("\n");
	}

	buffer.append("#line 1\n");

	copy.insert(pos + 1, buffer);

	const char *s = copy.c_str();
	glShaderSource(shader, 1, &s, nullptr);
	glCompileShader(shader);
}

void CStdGLShaderProgram::Link()
{
	EnsureProgram();

	glLinkProgram(shaderProgram);

	GLint status = 0;
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &status);
	if (!status)
	{
		GLint size = 0;
		glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &size);
		assert(size);
		if (size)
		{
			std::string errorMessage;
			errorMessage.resize(size);
			glGetProgramInfoLog(shaderProgram, size, NULL, errorMessage.data());
			throw Exception{errorMessage};
		}

		throw Exception{"Link failed"};
	}

	glValidateProgram(shaderProgram);
	glGetProgramiv(shaderProgram, GL_VALIDATE_STATUS, &status);
	if (!status)
	{
		GLint size = 0;
		glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &size);
		if (size)
		{
			std::string errorMessage;
			errorMessage.resize(size);
			glGetProgramInfoLog(shaderProgram, size, NULL, errorMessage.data());
			throw Exception{errorMessage};
		}

		throw Exception{"Validation failed"};
	}

	for (const auto &shader : shaders)
	{
		glDetachShader(shaderProgram, dynamic_cast<CStdGLShader *>(shader)->GetHandle());
	}

	shaders.clear();
}

void CStdGLShaderProgram::Clear()
{
	for (const auto &shader : shaders)
	{
		glDetachShader(shaderProgram, dynamic_cast<CStdGLShader *>(shader)->GetHandle());
	}

	if (shaderProgram)
	{
		glDeleteProgram(shaderProgram);
		shaderProgram = 0;
	}

	attributeLocations.clear();
	uniformLocations.clear();

	CStdShaderProgram::Clear();
}

void CStdGLShaderProgram::EnsureProgram()
{
	if (!shaderProgram)
	{
		shaderProgram = glCreateProgram();
	}
	assert(shaderProgram);
}

bool CStdGLShaderProgram::AddShaderInt(CStdShader *shader)
{
	if (auto *s = dynamic_cast<CStdGLShader *>(shader); s)
	{
		glAttachShader(shaderProgram, s->GetHandle());
		return true;
	}

	return false;
}

void CStdGLShaderProgram::OnSelect()
{
	assert(shaderProgram);
	glUseProgram(shaderProgram);
}

void CStdGLShaderProgram::OnDeselect()
{
	glUseProgram(GL_NONE);
}

static void glColorDw(const uint32_t dwClr)
{
	glColor4ub(
		static_cast<GLubyte>(dwClr >> 16),
		static_cast<GLubyte>(dwClr >> 8),
		static_cast<GLubyte>(dwClr),
		static_cast<GLubyte>(dwClr >> 24));
}

CStdGL::CStdGL()
{
	Default();
	// global ptr
	pGL = this;
}

CStdGL::~CStdGL()
{
	Clear();
	pGL = nullptr;
}

void CStdGL::Clear()
{
#ifndef USE_SDL_MAINLOOP
	CStdDDraw::Clear();
#endif
	NoPrimaryClipper();
	if (pTexMgr) pTexMgr->IntUnlock();
	InvalidateDeviceObjects();
	NoPrimaryClipper();
	// del font objects
	// del main surfaces
	delete lpPrimary;
	lpPrimary = lpBack = nullptr;
	RenderTarget = nullptr;
	// clear context
	if (pCurrCtx) pCurrCtx->Deselect();
	MainCtx.Clear();
	pCurrCtx = nullptr;
#ifndef USE_SDL_MAINLOOP
	CStdDDraw::Clear();
#endif
}

bool CStdGL::PageFlip(RECT *, RECT *, CStdWindow *)
{
	// call from gfx thread only!
	if (!pApp || !pApp->AssertMainThread()) return false;
	// safety
	if (!pCurrCtx) return false;
	// end the scene and present it
	if (!pCurrCtx->PageFlip()) return false;
	// success!
	return true;
}

void CStdGL::FillBG(const uint32_t dwClr)
{
	if (!pCurrCtx && !MainCtx.Select()) return;
	glClearColor(
		GetBValue(dwClr) / 255.0f,
		GetGValue(dwClr) / 255.0f,
		GetRValue(dwClr) / 255.0f,
		1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

bool CStdGL::UpdateClipper()
{
	int iX, iY, iWdt, iHgt;
	// no render target or clip all? do nothing
	if (!CalculateClipper(&iX, &iY, &iWdt, &iHgt)) return true;
	const auto scale = pApp->GetScale();
	glLineWidth(scale);
	glPointSize(scale);
	// set it
	glViewport(static_cast<int32_t>(floorf(iX * scale)), static_cast<int32_t>(floorf((RenderTarget->Hgt - iY - iHgt) * scale)), static_cast<int32_t>(ceilf(iWdt * scale)), static_cast<int32_t>(ceilf(iHgt * scale)));
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(
		static_cast<GLdouble>(iX),        static_cast<GLdouble>(iX + iWdt),
		static_cast<GLdouble>(iY + iHgt), static_cast<GLdouble>(iY));
	return true;
}

bool CStdGL::PrepareRendering(C4Surface *const sfcToSurface)
{
	// call from gfx thread only!
	if (!pApp || !pApp->AssertMainThread()) return false;
	// device?
	if (!pCurrCtx && !MainCtx.Select()) return false;
	// not ready?
	if (!Active) return false;
	// target?
	if (!sfcToSurface) return false;
	// target locked?
	assert(!sfcToSurface->Locked);
	// target is already set as render target?
	if (sfcToSurface != RenderTarget)
	{
		// target is a render-target?
		if (!sfcToSurface->IsRenderTarget()) return false;
		// set target
		RenderTarget = sfcToSurface;
		// new target has different size; needs other clipping rect
		UpdateClipper();
	}
	// done
	return true;
}

void CStdGL::PerformBlt(CBltData &rBltData, C4TexRef *const pTex,
	const uint32_t dwModClr, bool fMod2, const bool fExact)
{
	// global modulation map
	uint32_t dwModMask = 0;
	bool fAnyModNotBlack;
	bool fModClr = false;
	if (fUseClrModMap && dwModClr)
	{
		fAnyModNotBlack = false;
		for (int i = 0; i < rBltData.byNumVertices; ++i)
		{
			float x = rBltData.vtVtx[i].ftx;
			float y = rBltData.vtVtx[i].fty;
			if (rBltData.pTransform)
			{
				rBltData.pTransform->TransformPoint(x, y);
			}
			rBltData.vtVtx[i].dwModClr = pClrModMap->GetModAt(static_cast<int>(x), static_cast<int>(y));
			if (rBltData.vtVtx[i].dwModClr >> 24) dwModMask = 0xff000000;
			ModulateClr(rBltData.vtVtx[i].dwModClr, dwModClr);
			if (rBltData.vtVtx[i].dwModClr) fAnyModNotBlack = true;
			if (rBltData.vtVtx[i].dwModClr != 0xffffff) fModClr = true;
		}
	}
	else
	{
		fAnyModNotBlack = !!dwModClr;
		for (int i = 0; i < rBltData.byNumVertices; ++i)
			rBltData.vtVtx[i].dwModClr = dwModClr;
		if (dwModClr != 0xffffff) fModClr = true;
	}
	// reset MOD2 for completely black modulations
	if (fMod2 && !fAnyModNotBlack) fMod2 = 0;
	if (BlitShader)
	{
		dwModMask = 0;
		if (fMod2 && BlitShaderMod2)
		{
			BlitShaderMod2.Select();
		}
		else
		{
			BlitShader.Select();
		}

		if (!fModClr) glColor4f(1.0f, 1.0f, 1.0f, 0.0f);
	}
	// modulated blit
	else if (fModClr)
	{
		if (fMod2 || ((dwModClr >> 24 || dwModMask) && !DDrawCfg.NoAlphaAdd))
		{
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB,      fMod2 ? GL_ADD_SIGNED : GL_MODULATE);
			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE,        fMod2 ? 2.0f : 1.0f);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA,    GL_ADD);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB,      GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB,      GL_PRIMARY_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA,    GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA,    GL_PRIMARY_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB,     GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB,     GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA,   GL_SRC_ALPHA);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA,   GL_SRC_ALPHA);
			dwModMask = 0;
		}
		else
		{
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE,        1.0f);
			dwModMask = 0xff000000;
		}
	}
	else
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 1.0f);
	}
	// set texture+modes
	glShadeModel((fUseClrModMap && fModClr && !DDrawCfg.NoBoxFades) ? GL_SMOOTH : GL_FLAT);
	glBindTexture(GL_TEXTURE_2D, pTex->texName);
	if (pApp->GetScale() != 1.f || (!fExact && !DDrawCfg.PointFiltering))
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}

	glMatrixMode(GL_TEXTURE);
	float matrix[16];
	matrix[0] = rBltData.TexPos.mat[0];  matrix[ 1] = rBltData.TexPos.mat[3];  matrix[ 2] = 0; matrix[ 3] = rBltData.TexPos.mat[6];
	matrix[4] = rBltData.TexPos.mat[1];  matrix[ 5] = rBltData.TexPos.mat[4];  matrix[ 6] = 0; matrix[ 7] = rBltData.TexPos.mat[7];
	matrix[8] = 0;                       matrix[ 9] = 0;                       matrix[10] = 1; matrix[11] = 0;
	matrix[12] = rBltData.TexPos.mat[2]; matrix[13] = rBltData.TexPos.mat[5];  matrix[14] = 0; matrix[15] = rBltData.TexPos.mat[8];
	glLoadMatrixf(matrix);

	glMatrixMode(GL_MODELVIEW);
	if (rBltData.pTransform)
	{
		const float *const mat = rBltData.pTransform->mat;
		matrix[ 0] = mat[0]; matrix[ 1] = mat[3]; matrix[ 2] = 0; matrix[ 3] = mat[6];
		matrix[ 4] = mat[1]; matrix[ 5] = mat[4]; matrix[ 6] = 0; matrix[ 7] = mat[7];
		matrix[ 8] = 0;      matrix[ 9] = 0;      matrix[10] = 1; matrix[11] = 0;
		matrix[12] = mat[2]; matrix[13] = mat[5]; matrix[14] = 0; matrix[15] = mat[8];
		glLoadMatrixf(matrix);
	}
	else
	{
		glLoadIdentity();
	}

	// draw polygon
	glBegin(GL_POLYGON);
	for (int i = 0; i < rBltData.byNumVertices; ++i)
	{
		const auto &vtx = rBltData.vtVtx[i];
		if (fModClr) glColorDw(vtx.dwModClr | dwModMask);
		glTexCoord2f(vtx.ftx, vtx.fty);
		glVertex2f(vtx.ftx, vtx.fty);
	}
	glEnd();
	glLoadIdentity();
	if (BlitShader)
	{
		CStdShaderProgram::Deselect();
	}

	if (pApp->GetScale() != 1.f || (!fExact && !DDrawCfg.PointFiltering))
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}
}

void CStdGL::BlitLandscape(C4Surface *const sfcSource, C4Surface *const sfcSource2,
	C4Surface *const sfcLiquidAnimation, const int fx, const int fy,
	C4Surface *const sfcTarget, const int tx, const int ty, const int wdt, const int hgt)
{
	// safety
	if (!sfcSource || !sfcTarget || !wdt || !hgt) return;
	assert(sfcTarget->IsRenderTarget());
	assert(!(dwBlitMode & C4GFXBLIT_MOD2));
	// bound
	if (ClipAll) return;
	// inside screen?
	if (wdt <= 0 || hgt <= 0) return;
	// prepare rendering to surface
	if (!PrepareRendering(sfcTarget)) return;
	// texture present?
	if (!sfcSource->ppTex) return;
	// blit with basesfc?
	// get involved texture offsets
	int iTexSize = sfcSource->iTexSize;
	const int iTexX = (std::max)(fx / iTexSize, 0);
	const int iTexY = (std::max)(fy / iTexSize, 0);
	const int iTexX2 = (std::min)((fx + wdt - 1) / iTexSize + 1, sfcSource->iTexX);
	const int iTexY2 = (std::min)((fy + hgt - 1) / iTexSize + 1, sfcSource->iTexY);
	// blit from all these textures
	SetTexture();
	if (sfcSource2)
	{
		glActiveTexture(GL_TEXTURE1);
		glEnable(GL_TEXTURE_2D);
		glActiveTexture(GL_TEXTURE2);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, (*sfcLiquidAnimation->ppTex)->texName);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glActiveTexture(GL_TEXTURE0);
	}
	uint32_t dwModMask = 0;
	if (LandscapeShader)
	{
		LandscapeShader.Select();

		if (sfcSource2)
		{
			static GLfloat value[4] = { -0.6f / 3, 0.0f, 0.6f / 3, 0.0f };
			value[0] += 0.05f; value[1] += 0.05f; value[2] += 0.05f;
			GLfloat mod[4];
			for (int i = 0; i < 3; ++i)
			{
				if (value[i] > 0.9f) value[i] = -0.3f;
				mod[i] = (value[i] > 0.3f ? 0.6f - value[i] : value[i]) / 3.0f;
			}
			mod[3] = 0;

			LandscapeShader.SetUniform("modulation", glUniform4fv, 1, mod);
		}
	}
	// texture environment
	else
	{
		if (DDrawCfg.NoAlphaAdd)
		{
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE,        1.0f);
			dwModMask = 0xff000000;
		}
		else
		{
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB,      GL_MODULATE);
			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE,        1.0f);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA,    GL_ADD);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB,      GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB,      GL_PRIMARY_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA,    GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA,    GL_PRIMARY_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB,     GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB,     GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA,   GL_SRC_ALPHA);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA,   GL_SRC_ALPHA);
			dwModMask = 0;
		}
	}
	// set texture+modes
	glShadeModel((fUseClrModMap && !DDrawCfg.NoBoxFades) ? GL_SMOOTH : GL_FLAT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();

	const uint32_t dwModClr = BlitModulated ? BlitModulateClr : 0xffffff;
	int chunkSize = iTexSize;
	if (fUseClrModMap && dwModClr)
	{
		chunkSize = std::min(iTexSize, 64);
	}

	for (int iY = iTexY; iY < iTexY2; ++iY)
	{
		for (int iX = iTexX; iX < iTexX2; ++iX)
		{
			// blit

			if (sfcSource2) glActiveTexture(GL_TEXTURE0);
			const auto *const pTex = *(sfcSource->ppTex + iY * sfcSource->iTexX + iX);
			// get current blitting offset in texture (beforing any last-tex-size-changes)
			const int iBlitX = iTexSize * iX;
			const int iBlitY = iTexSize * iY;
			glBindTexture(GL_TEXTURE_2D, pTex->texName);
			if (sfcSource2)
			{
				const auto *const pTex = *(sfcSource2->ppTex + iY * sfcSource2->iTexX + iX);
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, pTex->texName);
			}

			int maxXChunk = std::min<int>((fx + wdt - iBlitX - 1) / chunkSize + 1, iTexSize / chunkSize);
			int maxYChunk = std::min<int>((fy + hgt - iBlitY - 1) / chunkSize + 1, iTexSize / chunkSize);

			// size changed? recalc dependent, relevant (!) values
			if (iTexSize != pTex->iSize) iTexSize = pTex->iSize;
			for (int yChunk = std::max<int>((fy - iBlitY) / chunkSize, 0); yChunk < maxYChunk; ++yChunk)
			{
				for (int xChunk = std::max<int>((fx - iBlitX) / chunkSize, 0); xChunk < maxXChunk; ++xChunk)
				{
					int xOffset = xChunk * chunkSize;
					int yOffset = yChunk * chunkSize;
					// draw polygon
					glBegin(GL_POLYGON);
					// get new texture source bounds
					FLOAT_RECT fTexBlt;
					// get new dest bounds
					FLOAT_RECT tTexBlt;
					// set up blit data as rect
					fTexBlt.left = std::max<float>(static_cast<float>(xOffset), fx - iBlitX);
					fTexBlt.top  = std::max<float>(static_cast<float>(yOffset), fy - iBlitY);
					fTexBlt.right  = std::min<float>(fx + wdt - iBlitX, static_cast<float>(xOffset + chunkSize));
					fTexBlt.bottom = std::min<float>(fy + hgt - iBlitY, static_cast<float>(yOffset + chunkSize));

					tTexBlt.left = fTexBlt.left + iBlitX - fx + tx;
					tTexBlt.top  = fTexBlt.top + iBlitY - fy + ty;
					tTexBlt.right  = fTexBlt.right + iBlitX - fx + tx;
					tTexBlt.bottom = fTexBlt.bottom + iBlitY - fy + ty;
					float ftx[4]; float fty[4]; // blit positions
					ftx[0] = tTexBlt.left;  fty[0] = tTexBlt.top;
					ftx[1] = tTexBlt.right; fty[1] = tTexBlt.top;
					ftx[2] = tTexBlt.right; fty[2] = tTexBlt.bottom;
					ftx[3] = tTexBlt.left;  fty[3] = tTexBlt.bottom;
					float tcx[4]; float tcy[4]; // blit positions
					tcx[0] = fTexBlt.left;  tcy[0] = fTexBlt.top;
					tcx[1] = fTexBlt.right; tcy[1] = fTexBlt.top;
					tcx[2] = fTexBlt.right; tcy[2] = fTexBlt.bottom;
					tcx[3] = fTexBlt.left;  tcy[3] = fTexBlt.bottom;

					uint32_t fdwModClr[4]; // color modulation
					// global modulation map
					if (fUseClrModMap && dwModClr)
					{
						for (int i = 0; i < 4; ++i)
						{
							fdwModClr[i] = pClrModMap->GetModAt(
								static_cast<int>(ftx[i]), static_cast<int>(fty[i]));
							ModulateClr(fdwModClr[i], dwModClr);
						}
					}
					else
					{
						std::fill(fdwModClr, std::end(fdwModClr), dwModClr);
					}

					for (int i = 0; i < 4; ++i)
					{
						glColorDw(fdwModClr[i] | dwModMask);
						glTexCoord2f((tcx[i] + DDrawCfg.fTexIndent) / iTexSize,
							(tcy[i] + DDrawCfg.fTexIndent) / iTexSize);
						if (sfcSource2)
						{
							glMultiTexCoord2f(GL_TEXTURE1_ARB,
								(tcx[i] + DDrawCfg.fTexIndent) / iTexSize,
								(tcy[i] + DDrawCfg.fTexIndent) / iTexSize);
							glMultiTexCoord2f(GL_TEXTURE2_ARB,
								(tcx[i] + DDrawCfg.fTexIndent) / sfcLiquidAnimation->iTexSize,
								(tcy[i] + DDrawCfg.fTexIndent) / sfcLiquidAnimation->iTexSize);
						}
						glVertex2f(ftx[i] + DDrawCfg.fBlitOff, fty[i] + DDrawCfg.fBlitOff);
					}

					glEnd();
				}
			}
		}
	}
	if (LandscapeShader)
	{
		CStdShaderProgram::Deselect();
	}
	if (sfcSource2)
	{
		glActiveTexture(GL_TEXTURE1);
		glDisable(GL_TEXTURE_2D);
		glActiveTexture(GL_TEXTURE2);
		glDisable(GL_TEXTURE_2D);
		glActiveTexture(GL_TEXTURE0);
	}
	// reset texture
	ResetTexture();
}

bool CStdGL::CreateDirectDraw()
{
	Log("  Using OpenGL...");
	return true;
}

CStdGLCtx *CStdGL::CreateContext(CStdWindow *const pWindow, CStdApp *const pApp)
{
	// safety
	if (!pWindow) return nullptr;

#ifdef USE_SDL_MAINLOOP
	if (SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1) != 0)
	{
		LogF("  SDL: Enabling context sharing failed: %s", SDL_GetError());
	}
#endif

	// create it
	const auto pCtx = new CStdGLCtx();
	if (!pCtx->Init(pWindow, pApp))
	{
		delete pCtx; Error("  gl: Error creating secondary context!"); return nullptr;
	}
	// done
	return pCtx;
}

#ifdef _WIN32
CStdGLCtx *CStdGL::CreateContext(const HWND hWindow, CStdApp *const pApp)
{
	// safety
	if (!hWindow) return nullptr;
	// create it
	const auto pCtx = new CStdGLCtx();
	if (!pCtx->Init(nullptr, pApp, hWindow))
	{
		delete pCtx; Error("  gl: Error creating secondary context!"); return nullptr;
	}
	// done
	return pCtx;
}
#endif

bool CStdGL::CreatePrimarySurfaces()
{
	// create lpPrimary and lpBack (used in first context selection)
	lpPrimary = lpBack = new C4Surface();
	lpPrimary->fPrimary = true;
	lpPrimary->AttachSfc(nullptr);

	// create+select gl context
	if (!MainCtx.Init(pApp->pWindow, pApp)) return Error("  gl: Error initializing context");

	// done, init device stuff
	return RestoreDeviceObjects();
}

void CStdGL::DrawQuadDw(C4Surface *const sfcTarget, int *const ipVtx,
	uint32_t dwClr1, uint32_t dwClr2, uint32_t dwClr3, uint32_t dwClr4)
{
	// prepare rendering to target
	if (!PrepareRendering(sfcTarget)) return;

	CStdGLShaderProgram::Deselect();

	// apply global modulation
	ClrByCurrentBlitMod(dwClr1);
	ClrByCurrentBlitMod(dwClr2);
	ClrByCurrentBlitMod(dwClr3);
	ClrByCurrentBlitMod(dwClr4);
	// apply modulation map
	if (fUseClrModMap)
	{
		ModulateClr(dwClr1, pClrModMap->GetModAt(ipVtx[0], ipVtx[1]));
		ModulateClr(dwClr2, pClrModMap->GetModAt(ipVtx[2], ipVtx[3]));
		ModulateClr(dwClr3, pClrModMap->GetModAt(ipVtx[4], ipVtx[5]));
		ModulateClr(dwClr4, pClrModMap->GetModAt(ipVtx[6], ipVtx[7]));
	}
	// no clr fading supported
	if (DDrawCfg.NoBoxFades)
	{
		NormalizeColors(dwClr1, dwClr2, dwClr3, dwClr4);
		glShadeModel(GL_FLAT);
	}
	else
		glShadeModel((dwClr1 == dwClr2 && dwClr1 == dwClr3 && dwClr1 == dwClr4) ? GL_FLAT : GL_SMOOTH);
	// set blitting state
	const int iAdditive = dwBlitMode & C4GFXBLIT_ADDITIVE;
	glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, iAdditive ? GL_ONE : GL_SRC_ALPHA);
	// draw two triangles
	glBegin(GL_POLYGON);
	glColorDw(dwClr1); glVertex2f(ipVtx[0] + DDrawCfg.fBlitOff, ipVtx[1] + DDrawCfg.fBlitOff);
	glColorDw(dwClr2); glVertex2f(ipVtx[2] + DDrawCfg.fBlitOff, ipVtx[3] + DDrawCfg.fBlitOff);
	glColorDw(dwClr3); glVertex2f(ipVtx[4] + DDrawCfg.fBlitOff, ipVtx[5] + DDrawCfg.fBlitOff);
	glColorDw(dwClr4); glVertex2f(ipVtx[6] + DDrawCfg.fBlitOff, ipVtx[7] + DDrawCfg.fBlitOff);
	glEnd();
	glShadeModel(GL_FLAT);
}

void CStdGL::DrawLineDw(C4Surface *const sfcTarget,
	const float x1, const float y1, const float x2, const float y2, uint32_t dwClr)
{
	// apply color modulation
	ClrByCurrentBlitMod(dwClr);
	// render target?
	assert(sfcTarget->IsRenderTarget());
	// prepare rendering to target
	if (!PrepareRendering(sfcTarget)) return;

	CStdGLShaderProgram::Deselect();

	// set blitting state
	const int iAdditive = dwBlitMode & C4GFXBLIT_ADDITIVE;
	// use a different blendfunc here, because GL_LINE_SMOOTH expects this one
	glBlendFunc(GL_SRC_ALPHA, iAdditive ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
	// draw one line
	glBegin(GL_LINES);
	// global clr modulation map
	uint32_t dwClr1 = dwClr;
	if (fUseClrModMap)
	{
		ModulateClr(dwClr1, pClrModMap->GetModAt(
			static_cast<int>(x1), static_cast<int>(y1)));
	}
	// convert from clonk-alpha to GL_LINE_SMOOTH alpha
	glColorDw(InvertRGBAAlpha(dwClr1));
	glVertex2f(x1 + 0.5f, y1 + 0.5f);
	if (fUseClrModMap)
	{
		ModulateClr(dwClr, pClrModMap->GetModAt(
			static_cast<int>(x2), static_cast<int>(y2)));
		glColorDw(InvertRGBAAlpha(dwClr));
	}
	glVertex2f(x2 + 0.5f, y2 + 0.5f);
	glEnd();
}

void CStdGL::DrawPixInt(C4Surface *const sfcTarget,
	const float tx, const float ty, const uint32_t dwClr)
{
	// render target?
	assert(sfcTarget->IsRenderTarget());

	if (!PrepareRendering(sfcTarget)) return;

	CStdGLShaderProgram::Deselect();

	const int iAdditive = dwBlitMode & C4GFXBLIT_ADDITIVE;
	// use a different blendfunc here because of GL_POINT_SMOOTH
	glBlendFunc(GL_SRC_ALPHA, iAdditive ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
	// convert the alpha value for that blendfunc
	glBegin(GL_POINTS);
	glColorDw(InvertRGBAAlpha(dwClr));
	glVertex2f(tx + 0.5f, ty + 0.5f);
	glEnd();
}

static void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam)
{
	LogF("source: %d, type: %d, id: %ul, severity: %d, message: %s\n", source, type, id, severity, message);
}

bool CStdGL::RestoreDeviceObjects()
{
	// safety
	if (!lpPrimary) return false;
	// delete any previous objects
	InvalidateDeviceObjects();
	// restore primary/back
	RenderTarget = lpPrimary;
	lpPrimary->AttachSfc(nullptr);

	// set states
	const bool fSuccess = pCurrCtx ? pCurrCtx->Select() : MainCtx.Select();
	// activate if successful
	Active = fSuccess;
	// restore gamma if active
	if (Active) EnableGamma();
	// reset blit states
	dwBlitMode = 0;

	if (DDrawCfg.Shader && !BlitShader)
	{
		if (glDebugMessageCallback)
		{
			glEnable(GL_DEBUG_OUTPUT);
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			glDebugMessageCallback(&MessageCallback, nullptr);
		}

		try
		{
			CStdGLShader vertexShader{CStdShader::Type::Vertex,
				R"(
				#version 120

				void main()
				{
					gl_Position = ftransform();
					gl_FrontColor = gl_Color;
					gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;
				#ifdef LC_COLOR_ANIMATION
					gl_TexCoord[1] = gl_TextureMatrix[0] * gl_MultiTexCoord1;
					gl_TexCoord[2] = gl_TextureMatrix[0] * gl_MultiTexCoord2;
				#endif
				}
				)"
			};

			vertexShader.Compile();

			CStdGLShader blitFragmentShader{CStdShader::Type::Fragment,
				R"(
				#version 120

				uniform sampler2D textureSampler;

				void main()
				{
					vec4 fragColor = texture2D(textureSampler, gl_TexCoord[0].st);

				#ifdef LC_MOD2
					fragColor.rgb += gl_Color.rgb;
					fragColor.rgb = clamp(fragColor.rgb * 2.0 - 1.0, 0.0, 1.0);
				#else
					fragColor.rgb *= gl_Color.rgb;
					fragColor.rgb = clamp(fragColor.rgb, 0.0, 1.0);
					fragColor.a = clamp(fragColor.a + gl_Color.a, 0.0, 1.0);
				#endif
					gl_FragColor = fragColor;
				}
				)"
			};

			blitFragmentShader.Compile();

			BlitShader.AddShader(&vertexShader);
			BlitShader.AddShader(&blitFragmentShader);
			BlitShader.Link();

			blitFragmentShader.SetMacro("LC_MOD2", "1");
			blitFragmentShader.Compile();

			BlitShaderMod2.AddShader(&vertexShader);
			BlitShaderMod2.AddShader(&blitFragmentShader);
			BlitShaderMod2.Link();

			CStdGLShader landscapeFragmentShader{CStdShader::Type::Fragment,
				R"(
				#version 120

				uniform sampler2D textureSampler;
				#ifdef LC_COLOR_ANIMATION
				uniform sampler2D maskSampler;
				uniform sampler2D liquidSampler;
				uniform vec4 modulation;
				#endif

				void main()
				{
					vec4 fragColor = texture2D(textureSampler,  gl_TexCoord[0].st);
				#ifdef LC_COLOR_ANIMATION
					float mask = texture2D(maskSampler,  gl_TexCoord[1].st).a;
					vec3 liquid = texture2D(liquidSampler,  gl_TexCoord[2].st).rgb;
					liquid -= vec3(0.5, 0.5, 0.5);
					liquid = vec3(dot(liquid, modulation.rgb));
					liquid *= mask;
					fragColor.rgb = fragColor.rgb + liquid;
				#endif
					fragColor.rgb = clamp(fragColor.rgb, 0.0, 1.0) * gl_Color.rgb;
					fragColor.a = clamp(fragColor.a + gl_Color.a, 0.0, 1.0);

					gl_FragColor = fragColor;
				}
				)"};

			if (Config.Graphics.ColorAnimation)
			{
				vertexShader.SetMacro("LC_COLOR_ANIMATION", "1");
				vertexShader.Compile();
				landscapeFragmentShader.SetMacro("LC_COLOR_ANIMATION", "1");
			}

			landscapeFragmentShader.Compile();

			LandscapeShader.AddShader(&vertexShader);
			LandscapeShader.AddShader(&landscapeFragmentShader);
			LandscapeShader.Link();

			for (auto *const shader : {&BlitShader, &BlitShaderMod2, &LandscapeShader})
			{
				shader->Select();
				shader->SetUniform("texIndent", DDrawCfg.fTexIndent);
				shader->SetUniform("blitOffset", DDrawCfg.fBlitOff);
				shader->SetUniform("textureSampler", glUniform1i, 0);

				if (shader == &LandscapeShader)
				{
					shader->SetUniform("maskSampler", glUniform1i, 1);
					shader->SetUniform("liquidSampler", glUniform1i, 2);
				}
			}

			CStdShaderProgram::Deselect();
		}
		catch (const CStdRenderException &e)
		{
			LogFatal(e.what());
			return Active = false;
		}
	}
	// done
	return Active;
}

bool CStdGL::InvalidateDeviceObjects()
{
	// clear gamma
#ifndef USE_SDL_MAINLOOP
	DisableGamma();
#endif
	// deactivate
	Active = false;
	// invalidate font objects
	// invalidate primary surfaces
	if (lpPrimary) lpPrimary->Clear();
	if (BlitShader)
	{
		BlitShader.Clear();
		BlitShaderMod2.Clear();
		LandscapeShader.Clear();
	}
	return true;
}

void CStdGL::SetTexture()
{
	glBlendFunc(GL_ONE_MINUS_SRC_ALPHA,
		(dwBlitMode & C4GFXBLIT_ADDITIVE) ? GL_ONE : GL_SRC_ALPHA);
	glEnable(GL_TEXTURE_2D);
}

void CStdGL::ResetTexture()
{
	// disable texturing
	glDisable(GL_TEXTURE_2D);
}

CStdGL *pGL = nullptr;

bool CStdGL::OnResolutionChanged()
{
	InvalidateDeviceObjects();
	RestoreDeviceObjects();
	// Re-create primary clipper to adapt to new size.
	CreatePrimaryClipper();
	return true;
}

void CStdGL::Default()
{
	CStdDDraw::Default();
	sfcFmt = 0;
	MainCtx.Clear();
}

#endif
