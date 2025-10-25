///////////////////////////////////////////////////////////////////////////////
// shadermanager.cpp
// ============
// manage the loading and rendering of 3D scenes
//
//  AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
//	Created for CS-330-Computational Graphics and Visualization, Nov. 1st, 2023
///////////////////////////////////////////////////////////////////////////////

#include "SceneManager.h"
#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif
#include <glm/gtx/transform.hpp>
#include <filesystem>

static std::string FindTexturesBase()
{
	namespace fs = std::filesystem;

	// Print the current working dir once for sanity
	static bool printed = false;
	if (!printed) {
		std::cout << "CWD: " << fs::current_path() << std::endl;
		printed = true;
	}

	// Try a few likely bases (first that contains wood_oak.jpg wins)
	const char* candidates[] = {
		"../Utilities/textures/",
		"../../Utilities/textures/",
		"./Utilities/textures/",
		"./textures/",
		"../textures/",
		"../../../Utilities/textures/"
	};

	for (auto base : candidates)
	{
		if (fs::exists(fs::path(base) / "wood_oak.jpg"))
			return base;
	}

	std::cout << "Could not locate textures folder from CWD" << std::endl;
	return ""; // fall through; CreateGLTexture will then print errors
}

// declaration of global variables
namespace
{
	const char* g_ModelName = "model";
	const char* g_ColorValueName = "objectColor";
	const char* g_TextureValueName = "objectTexture";
	const char* g_UseTextureName = "bUseTexture";
	const char* g_UseLightingName = "bUseLighting";
}

/***********************************************************
 *  SceneManager()
 *
 *  The constructor for the class
 ***********************************************************/
SceneManager::SceneManager(ShaderManager* pShaderManager)
{
	m_pShaderManager = pShaderManager;
	m_basicMeshes = new ShapeMeshes();
	m_loadedTextures = 0;        // <<< add this
}

/***********************************************************
 *  ~SceneManager()
 *
 *  The destructor for the class
 ***********************************************************/
SceneManager::~SceneManager()
{
	m_pShaderManager = NULL;
	delete m_basicMeshes;
	m_basicMeshes = NULL;
}

/***********************************************************
 *  CreateGLTexture()
 *
 *  This method is used for loading textures from image files,
 *  configuring the texture mapping parameters in OpenGL,
 *  generating the mipmaps, and loading the read texture into
 *  the next available texture slot in memory.
 ***********************************************************/
bool SceneManager::CreateGLTexture(const char* filename, std::string tag)
{
	int width = 0, height = 0, colorChannels = 0;
	GLuint textureID = 0;

	stbi_set_flip_vertically_on_load(true);

	// Force-convert to RGBA to avoid CMYK/odd formats
	unsigned char* image = stbi_load(filename, &width, &height, &colorChannels, STBI_rgb_alpha);
	if (!image)
	{
		std::cout << "Could not load image:" << filename
			<< "  reason: " << stbi_failure_reason() << std::endl;
		return false;
	}

	std::cout << "Loaded image: " << filename
		<< "  w:" << width << " h:" << height << " -> RGBA\n";

	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);

	// Wrapping
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	// Filtering (trilinear)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Upload always as RGBA now
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, image);
	glGenerateMipmap(GL_TEXTURE_2D);

	stbi_image_free(image);
	glBindTexture(GL_TEXTURE_2D, 0);

	// Register
	m_textureIDs[m_loadedTextures].ID = textureID;
	m_textureIDs[m_loadedTextures].tag = tag;
	std::cout << "Registered texture '" << tag << "' in slot "
		<< m_loadedTextures << ", GL id " << textureID << "\n";
	m_loadedTextures++;
	return true;
}

/***********************************************************
 *  BindGLTextures()
 *
 *  This method is used for binding the loaded textures to
 *  OpenGL texture memory slots.  There are up to 16 slots.
 ***********************************************************/
void SceneManager::BindGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		// bind textures on corresponding texture units
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, m_textureIDs[i].ID);
	}
}

/***********************************************************
 *  DestroyGLTextures()
 *
 *  This method is used for freeing the memory in all the
 *  used texture memory slots.
 ***********************************************************/
void SceneManager::DestroyGLTextures()
{
	for (int i = 0; i < m_loadedTextures; ++i)
		glDeleteTextures(1, &m_textureIDs[i].ID);  // <<< fix
	m_loadedTextures = 0;
}

/***********************************************************
 *  FindTextureID()
 *
 *  This method is used for getting an ID for the previously
 *  loaded texture bitmap associated with the passed in tag.
 ***********************************************************/
int SceneManager::FindTextureID(std::string tag)
{
	int textureID = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureID = m_textureIDs[index].ID;
			bFound = true;
		}
		else
			index++;
	}

	return(textureID);
}

/***********************************************************
 *  FindTextureSlot()
 *
 *  This method is used for getting a slot index for the previously
 *  loaded texture bitmap associated with the passed in tag.
 ***********************************************************/
int SceneManager::FindTextureSlot(std::string tag)
{
	int textureSlot = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureSlot = index;
			bFound = true;
		}
		else
			index++;
	}

	return(textureSlot);
}

/***********************************************************
 *  FindMaterial()
 *
 *  This method is used for getting a material from the previously
 *  defined materials list that is associated with the passed in tag.
 ***********************************************************/
bool SceneManager::FindMaterial(std::string tag, OBJECT_MATERIAL& material)
{
	if (m_objectMaterials.size() == 0)
	{
		return(false);
	}

	int index = 0;
	bool bFound = false;
	while ((index < m_objectMaterials.size()) && (bFound == false))
	{
		if (m_objectMaterials[index].tag.compare(tag) == 0)
		{
			bFound = true;
			material.ambientColor = m_objectMaterials[index].ambientColor;
			material.ambientStrength = m_objectMaterials[index].ambientStrength;
			material.diffuseColor = m_objectMaterials[index].diffuseColor;
			material.specularColor = m_objectMaterials[index].specularColor;
			material.shininess = m_objectMaterials[index].shininess;
		}
		else
		{
			index++;
		}
	}

	return(true);
}

void SceneManager::LoadSceneTextures()
{
	m_loadedTextures = 0;

	const std::string base = FindTexturesBase();

	CreateGLTexture((base + "wood_oak.jpg").c_str(), "TEX_WOOD");
	CreateGLTexture((base + "black_plastic.jpg").c_str(), "TEX_PLASTIC");
	CreateGLTexture((base + "fabric_dark.jpg").c_str(), "TEX_FABRIC");
	CreateGLTexture((base + "paint_wall.jpg").c_str(), "TEX_WALL");
	CreateGLTexture((base + "carpet.jpg").c_str(), "TEX_CARPET");
	CreateGLTexture((base + "monitor_bezel.png").c_str(), "TEX_BEZEL");
	CreateGLTexture((base + "monitor_screen.jpg").c_str(), "TEX_SCREEN");
	CreateGLTexture((base + "gloss_reflection.png").c_str(), "TEX_GLOSS");

	BindGLTextures();
}

/***********************************************************
 *  SetTransformations()
 *
 *  This method is used for setting the transform buffer
 *  using the passed in transformation values.
 ***********************************************************/
void SceneManager::SetTransformations(
	glm::vec3 scaleXYZ,
	float XrotationDegrees,
	float YrotationDegrees,
	float ZrotationDegrees,
	glm::vec3 positionXYZ)
{
	// variables for this method
	glm::mat4 modelView;
	glm::mat4 scale;
	glm::mat4 rotationX;
	glm::mat4 rotationY;
	glm::mat4 rotationZ;
	glm::mat4 translation;

	// set the scale value in the transform buffer
	scale = glm::scale(scaleXYZ);
	// set the rotation values in the transform buffer
	rotationX = glm::rotate(glm::radians(XrotationDegrees), glm::vec3(1.0f, 0.0f, 0.0f));
	rotationY = glm::rotate(glm::radians(YrotationDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
	rotationZ = glm::rotate(glm::radians(ZrotationDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
	// set the translation value in the transform buffer
	translation = glm::translate(positionXYZ);

	modelView = translation * rotationX * rotationY * rotationZ * scale;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setMat4Value(g_ModelName, modelView);
	}
}

/***********************************************************
 *  SetShaderColor()
 *
 *  This method is used for setting the passed in color
 *  into the shader for the next draw command
 ***********************************************************/
void SceneManager::SetShaderColor(
	float redColorValue,
	float greenColorValue,
	float blueColorValue,
	float alphaValue)
{
	// variables for this method
	glm::vec4 currentColor;

	currentColor.r = redColorValue;
	currentColor.g = greenColorValue;
	currentColor.b = blueColorValue;
	currentColor.a = alphaValue;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, false);
		m_pShaderManager->setVec4Value(g_ColorValueName, currentColor);
	}
}

/***********************************************************
 *  SetShaderTexture()
 *
 *  This method is used for setting the texture data
 *  associated with the passed in ID into the shader.
 ***********************************************************/
void SceneManager::SetShaderTexture(
	std::string textureTag)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, true);

		int textureID = -1;
		textureID = FindTextureSlot(textureTag);
		m_pShaderManager->setSampler2DValue(g_TextureValueName, textureID);
	}
}

/***********************************************************
 *  SetTextureUVScale()
 *
 *  This method is used for setting the texture UV scale
 *  values into the shader.
 ***********************************************************/
void SceneManager::SetTextureUVScale(float u, float v)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setVec2Value("UVscale", glm::vec2(u, v));
	}
}

/***********************************************************
 *  SetShaderMaterial()
 *
 *  This method is used for passing the material values
 *  into the shader.
 ***********************************************************/
void SceneManager::SetShaderMaterial(
	std::string materialTag)
{
	if (m_objectMaterials.size() > 0)
	{
		OBJECT_MATERIAL material;
		bool bReturn = false;

		bReturn = FindMaterial(materialTag, material);
		if (bReturn == true)
		{
			m_pShaderManager->setVec3Value("material.ambientColor", material.ambientColor);
			m_pShaderManager->setFloatValue("material.ambientStrength", material.ambientStrength);
			m_pShaderManager->setVec3Value("material.diffuseColor", material.diffuseColor);
			m_pShaderManager->setVec3Value("material.specularColor", material.specularColor);
			m_pShaderManager->setFloatValue("material.shininess", material.shininess);
		}
	}
}

/**************************************************************/
/*** STUDENTS CAN MODIFY the code in the methods BELOW for  ***/
/*** preparing and rendering their own 3D replicated scenes.***/
/*** Please refer to the code in the OpenGL sample project  ***/
/*** for assistance.                                        ***/
/**************************************************************/


/***********************************************************
 *  PrepareScene()
 *
 *  This method is used for preparing the 3D scene by loading
 *  the shapes, textures in memory to support the 3D scene 
 *  rendering
 ***********************************************************/
void SceneManager::PrepareScene()
{
	// Load each primitive once; reuse in RenderScene
	m_basicMeshes->LoadPlaneMesh();
	m_basicMeshes->LoadBoxMesh();
	m_basicMeshes->LoadCylinderMesh();
	m_basicMeshes->LoadSphereMesh();
	LoadSceneTextures();
}

/***********************************************************
 *  RenderScene()
 *
 *  This method is used for rendering the 3D scene by 
 *  transforming and drawing the basic 3D shapes
 ***********************************************************/
void SceneManager::RenderScene()
{
	// ---------- helpers ----------
	auto DrawBox = [&](glm::vec3 S, glm::vec3 Rdeg, glm::vec3 T, glm::vec4 RGBA)
		{ SetTransformations(S, Rdeg.x, Rdeg.y, Rdeg.z, T); SetShaderColor(RGBA.r, RGBA.g, RGBA.b, RGBA.a); m_basicMeshes->DrawBoxMesh(); };
	auto DrawCyl = [&](glm::vec3 S, glm::vec3 Rdeg, glm::vec3 T, glm::vec4 RGBA)
		{ SetTransformations(S, Rdeg.x, Rdeg.y, Rdeg.z, T); SetShaderColor(RGBA.r, RGBA.g, RGBA.b, RGBA.a); m_basicMeshes->DrawCylinderMesh(); };
	auto DrawSphere = [&](glm::vec3 S, glm::vec3 Rdeg, glm::vec3 T, glm::vec4 RGBA)
		{ SetTransformations(S, Rdeg.x, Rdeg.y, Rdeg.z, T); SetShaderColor(RGBA.r, RGBA.g, RGBA.b, RGBA.a); m_basicMeshes->DrawSphereMesh(); };
	auto DrawPlane = [&](glm::vec3 S, glm::vec3 Rdeg, glm::vec3 T, glm::vec4 RGBA)
		{ SetTransformations(S, Rdeg.x, Rdeg.y, Rdeg.z, T); SetShaderColor(RGBA.r, RGBA.g, RGBA.b, RGBA.a); m_basicMeshes->DrawPlaneMesh(); };

	// ------ texture helpers (no dimension changes) ------
	auto DrawBoxTex = [&](glm::vec3 S, glm::vec3 Rdeg, glm::vec3 T,
		const char* tag, glm::vec2 uv = { 1,1 }, float a = 1.0f)
		{
			SetTransformations(S, Rdeg.x, Rdeg.y, Rdeg.z, T);
			SetTextureUVScale(uv.x, uv.y);
			SetShaderColor(1, 1, 1, a);    // tint first
			SetShaderTexture(tag);         // enable texture last
			m_basicMeshes->DrawBoxMesh();
			SetTextureUVScale(1, 1);
		};

	auto DrawPlaneTex = [&](glm::vec3 S, glm::vec3 Rdeg, glm::vec3 T,
		const char* tag, glm::vec2 uv = { 1,1 }, float a = 1.0f)
		{
			SetTransformations(S, Rdeg.x, Rdeg.y, Rdeg.z, T);
			SetTextureUVScale(uv.x, uv.y);
			SetShaderColor(1, 1, 1, a);
			SetShaderTexture(tag);
			m_basicMeshes->DrawPlaneMesh();
			SetTextureUVScale(1, 1);
		};

	// ---------- palette (kept) ----------
	const glm::vec4 BLACK = { 0.05f, 0.05f, 0.06f, 1.0f };
	const glm::vec4 DESK = { 0.18f, 0.18f, 0.20f, 1.0f };
	const glm::vec4 WALLFLR = { 0.40f, 0.40f, 0.42f, 1.0f };
	const glm::vec4 WHITE = { 0.92f, 0.92f, 0.94f, 1.0f };
	const glm::vec4 GREEN = { 0.10f, 0.90f, 0.20f, 1.0f };
	const glm::vec4 MPAD = { 0.12f, 0.12f, 0.14f, 1.0f };

	// ---------- global scale ----------
	const float SALL = 1.30f;
	const float pairX = 0.38f;

	// ---------- back wall & floor (textured) ----------
	DrawBoxTex({ 4.0f, 2.2f, 0.03f }, { 0,0,0 }, { 0.0f, 1.1f, -0.80f }, "TEX_WALL", { 3.0f,1.5f });
	DrawPlaneTex({ 8.0f, 1.0f, 8.0f }, { 0,0,0 }, { 0.0f, -0.002f, 0.0f }, "TEX_CARPET", { 6.0f,6.0f });

	// ---------- desk (textured wood, same dims) ----------
	const glm::vec3 deskS = { 1.60f, 0.03f, 0.60f };
	const float deskHalfH = deskS.y * 0.5f;
	DrawBoxTex(deskS, { 0,0,0 }, { 0.0f, 0.0f, 0.0f }, "TEX_WOOD", { 4.0f,1.5f });
	const float deskTopY = deskHalfH;

	// ---------- shelf (textured wood, same dims) ----------
	const glm::vec3 shelfS = { 1.50f, 0.05f, 0.45f };
	const float shelfHalfH = shelfS.y * 0.5f;
	DrawBoxTex(shelfS, { 0,0,0 }, { 0.0f, 0.32f, -0.05f }, "TEX_WOOD", { 3.0f,1.0f });
	const float shelfTopY = 0.32f + shelfHalfH;

	// ---------- consoles (left plastic, right white color) ----------
	const glm::vec3 xbox1S = SALL * glm::vec3(0.33f, 0.08f, 0.27f);
	const glm::vec3 xbox3S = SALL * glm::vec3(0.31f, 0.08f, 0.26f);

	DrawBoxTex(xbox1S, { 0,0,0 }, { -pairX, shelfTopY + xbox1S.y * 0.5f, -0.08f }, "TEX_PLASTIC");
	const float xbox1TopY = shelfTopY + xbox1S.y;

	DrawBox(xbox3S, { 0,0,0 }, { pairX, shelfTopY + xbox3S.y * 0.5f, -0.08f }, WHITE);
	const float xbox3TopY = shelfTopY + xbox3S.y;

	// 360 power ring (unchanged)
	DrawCyl(SALL * glm::vec3(0.013f, 0.005f, 0.013f), { 90,0,0 },
		{ pairX + 0.12f, shelfTopY + (xbox3S.y * 0.5f), 0.02f }, GREEN);
	DrawSphere(SALL * glm::vec3(0.012f, 0.012f, 0.012f), { 0,0,0 },
		{ pairX + 0.12f, shelfTopY + (xbox3S.y * 0.5f), 0.027f }, WHITE);

	// ================= Stands + Panels (textured bezel + layered screen) =================
	const glm::vec3 panelS = SALL * glm::vec3(0.53f, 0.33f, 0.02f);
	const float     panelHalfH = panelS.y * 0.5f;
	const glm::vec3 standFootS = SALL * glm::vec3(0.20f, 0.02f, 0.12f);
	const float     postH = SALL * 0.03f;
	const float     postR = SALL * 0.020f;

	auto StandStack = [&](float baseX, float baseTopY)
		{
			// Foot (plastic texture)
			DrawBoxTex(standFootS, { 0,0,0 },
				{ baseX, baseTopY + standFootS.y * 0.5f, -0.05f }, "TEX_PLASTIC");

			// Post (matte black)
			DrawCyl({ postR, postH, postR }, { 0,0,0 },
				{ baseX, baseTopY + standFootS.y + postH * 0.0f, -0.05f }, BLACK);

			// Panel position (center of bezel)
			const float     supportTopY = baseTopY + standFootS.y + postH;
			const glm::vec3 panelPos = { baseX, supportTopY + panelHalfH, -0.05f };

			// Bezel (PNG alpha)
			DrawBoxTex(panelS, { 0,0,0 }, panelPos, "TEX_BEZEL");

			// --- draw the screen + gloss as ultra-thin BOXES (not planes) ---
			// Screen image, just in front of bezel
			DrawBoxTex(SALL * glm::vec3(0.495f, 0.315f, 0.0008f), { 0,0,0 },
				panelPos + glm::vec3(0, 0, 0.0118f), "TEX_SCREEN");

			// Gloss overlay: draw last, disable depth WRITES to avoid fighting
			glDepthMask(GL_FALSE);
			DrawBoxTex(SALL * glm::vec3(0.495f, 0.315f, 0.0006f), { 0,0,0 },
				panelPos + glm::vec3(0, 0, 0.0130f), "TEX_GLOSS", { 1,1 }, 0.35f);
			glDepthMask(GL_TRUE);
		};

	StandStack(-pairX, xbox1TopY);   // left
	StandStack(pairX, xbox3TopY);   // right
	// ================= end stands + panels ======================================

	// ---------- keyboard / mousepad / mouse ----------
	DrawBoxTex(SALL * glm::vec3(0.33f, 0.01f, 0.27f), { 0,0,0 },
		{ 0.55f, deskTopY + (SALL * 0.01f) * 0.5f, 0.05f }, "TEX_FABRIC", { 2.5f,2.0f });
	DrawBoxTex(SALL * glm::vec3(0.47f, 0.025f, 0.15f), { -3.0f, 10.0f, 0.0f },
		{ -0.10f, deskTopY + (SALL * 0.025f) * 0.5f, 0.06f }, "TEX_PLASTIC");
	DrawBox(SALL * glm::vec3(0.06f, 0.007f, 0.09f), { 0, -20.0f, 0 },
		{ 0.60f, deskTopY + (SALL * 0.007f) * 0.5f, 0.05f }, BLACK);
	DrawSphere(SALL * glm::vec3(0.05f, 0.025f, 0.075f), { 0, -20.0f, 0 },
		{ 0.60f, deskTopY + (SALL * 0.007f) + (SALL * 0.025f) * 0.5f + 0.004f, 0.05f }, BLACK);
}