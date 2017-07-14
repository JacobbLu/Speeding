#include "A3.hpp"
#include "scene_lua.hpp"
using namespace std;

#include "cs488-framework/GlErrorCheck.hpp"
#include "cs488-framework/MathUtils.hpp"
#include "GeometryNode.hpp"
#include "JointNode.hpp"
#include <iostream>
#include <imgui/imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace irrklang;
#pragma comment(lib, "irrKlang.lib")


#define PI 3.1415926

using namespace glm;

static bool show_gui = true;

const size_t CIRCLE_PTS = 48;
const float g = 0.005f;

enum modes {
    Position,
    Joint
};

bool circle_mode = false;
bool zbuffer_mode = true;
bool backface_mode = false;
bool frontface_mode = false;
bool oldframe = true;

bool Lclicking = false;
bool Mclicking = false;
bool Rclicking = false;
bool picking = false;
bool gameover = false;
int gameoverf = 0;
int Score = 0;
int HighScore = 0;
int Power = 0;//0 - 10000
float speedup = 0;


glm::vec2 oldxy;

mat4 TM;
mat4 OM;

float jumpspeed;
float jumping;
float jumpspeedh;
float airf;

vector<unsigned int> Selected;
mat4 Tarray[10000];


//----------------------------------------------------------------------------------------
// Constructor
VertexData::VertexData()
: numVertices(0),
index(0)
{
    positions.reserve(kMaxVertices);
    colours.reserve(kMaxVertices);
}

//----------------------------------------------------------------------------------------
// Constructor
A3::A3(const std::string & luaSceneFile)
	: m_luaSceneFile(luaSceneFile),
	  m_positionAttribLocation(0),
	  m_normalAttribLocation(0),
	  m_vao_meshData(0),
	  m_vbo_vertexPositions(0),
	  m_vbo_vertexNormals(0),
	  m_vao_arcCircle(0),
	  m_vbo_arcCircle(0),
      m_currentLineColour(vec3(0.0f))
{

}

//----------------------------------------------------------------------------------------
// Destructor
A3::~A3()
{

}

//----------------------------------------------------------------------------------------
/*
 * Called once, at program start.
 */
void A3::init()
{
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_LINE_SMOOTH);
    picking = false;
	// Set the background colour.
	glClearColor(0.20, 0.40, 0.13, 1.0);
    //glClearColor(0.0f, 0.0f, 0.0f, 1.0);

	createShaderProgram();

	glGenVertexArrays(1, &m_vao_arcCircle);
	glGenVertexArrays(1, &m_vao_meshData);
    glGenVertexArrays(1, &m_vao);

    
	enableVertexShaderInputSlots();
    
    generateVertexBuffers();

	processLuaSceneFile(m_luaSceneFile);

	// Load and decode all .obj files at once here.  You may add additional .obj files to
	// this list in order to support rendering additional mesh types.  All vertex
	// positions, and normals will be extracted and stored within the MeshConsolidator
	// class.
	unique_ptr<MeshConsolidator> meshConsolidator (new MeshConsolidator{
			getAssetFilePath("cube.obj"),
			getAssetFilePath("sphere.obj"),
			getAssetFilePath("suzanne.obj")
	});


	// Acquire the BatchInfoMap from the MeshConsolidator.
	meshConsolidator->getBatchInfoMap(m_batchInfoMap);

	// Take all vertex data within the MeshConsolidator and upload it to VBOs on the GPU.
	uploadVertexDataToVbos(*meshConsolidator);

	mapVboDataToVertexShaderInputLocations();

	initPerspectiveMatrix();

	initViewMatrix();

	initLightSources();
    
    Far = -1000.0f;
    Near = -0.0f;
    F = PI / 24;
    int Window_height;
    int Window_width;
    glfwGetWindowSize(m_window, &Window_width, &Window_height);
    Aspect_ratio = (float)Window_height / (float)Window_width;
    LaneFrame = 0.0f;
    
    ViewM = inverse(glm::mat4(vec4(1, 0, 0, 0), vec4(0, 1, 0, 0), vec4(0, 0, -1, 0), vec4(0, -5, 40, 1)));
    ProjM = glm::mat4(vec4((1.0f / tan(F / 2)) / Aspect_ratio, 0, 0, 0),
                      vec4(0, 1.0f / tan(F / 2), 0, 0),
                      vec4(0, 0, -(Far + Near) / (Far - Near), -1),
                      vec4(0, 0, (-2 * Far * Near) / (Far - Near), 0));
    
    jumpspeed = 0.0f;
    jumping = 0.0f;
    
    
    uploadCommonSceneUniforms();
    for(int i = 0; i < 10000; ++i){
        int a, b, c;
        a = rand()%80 - 40;
        b = rand()%200 - 40;
        c = rand()%80 - 40;
        Tarray[i] = translate(mat4(), vec3((float)a / 2000.0f, (float)b / 2000.0f, (float)c / 2000.0f));
    }
    // start the sound engine with default parameters
    ISoundEngine* engine = createIrrKlangDevice();
    engine->play2D("coolsound.mp3", true);



	// Exiting the current scope calls delete automatically on meshConsolidator freeing
	// all vertex data resources.  This is fine since we already copied this data to
	// VBOs on the GPU.  We have no use for storing vertex data on the CPU side beyond
	// this point.
}
//---------------------------------------------------------------------------------------
void A3::initLineData()
{
    m_vertexData.numVertices = 0;
    m_vertexData.index = 0;
}
//---------------------------------------------------------------------------------------
void A3::setLineColour (
                        const glm::vec3 & colour
                        ) {
    m_currentLineColour = colour;
}
//---------------------------------------------------------------------------------------
void A3::drawLine(
                  const glm::vec2 & v0,   // Line Start (NDC coordinate)
                  const glm::vec2 & v1    // Line End (NDC coordinate)
) {
    
    m_vertexData.positions[m_vertexData.index] = v0;
    m_vertexData.colours[m_vertexData.index] = m_currentLineColour;
    ++m_vertexData.index;
    m_vertexData.positions[m_vertexData.index] = v1;
    m_vertexData.colours[m_vertexData.index] = m_currentLineColour;
    ++m_vertexData.index;
    
    m_vertexData.numVertices += 2;
}
//----------------------------------------------------------------------------------------
/*
 * Draw the sky
 */
void A3::drawSky(){
    GLfloat verticesred[] =
    {
        -1.0f, 1.0f,   0,
        -1.0f, -0.046f,   0,
        1.0f, -0.046f,   0,
        
        1.0f, 1.0f,   0,
        1.0f, -0.046f,   0,
        -1.0f, 1.0f,   0
    };
    glGenVertexArrays(1, &pl_vao);
    glUniform3f(col_uni, 0.13f, 0.42f, 0.81f);
    glBindVertexArray(pl_vao);
    //vertices
    glGenBuffers(1, &pl_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, pl_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 9 * 2, verticesred, GL_STATIC_DRAW);
    
    GLint posAttribxx = pl_shader.getAttribLocation("position");
    glEnableVertexAttribArray(posAttribxx);
    glVertexAttribPointer(posAttribxx, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glDrawArrays(GL_TRIANGLES, 0, 3 * 2);
    
    glDeleteBuffers(1, &pl_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    CHECK_GL_ERRORS;
    
//    GLuint textureID = loadBMP_custom("./cs488.bmp");
//    glEnable(GL_TEXTURE_2D);
//    glBindTexture(GL_TEXTURE_2D, textureID);
//    GLint texCoordID = tex_shader.getAttribLocation("vertexUV");
//    glEnableVertexAttribArray(texCoordID);
//    glVertexAttribPointer(texCoordID, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
//    GLint texID = tex_shader.getUniformLocation("myTextureSampler");
//    glActiveTexture(GL_TEXTURE0);
//    glUniform1i(texID, 0);
    

}
//----------------------------------------------------------------------------------------
/*
 * Draw the road
 */
void A3::drawRoad(){
    vec4 leftnear = vec4(-5.0f, 0.0f, 10.0f, 1);
    vec4 leftfar = vec4(-5.0f, 0.0f, -1500.0f, 1);
    vec4 rightnear = vec4(5.0f, 0.0f, 10.0f, 1);
    vec4 rightfar = vec4(5.0f, 0.0f, -1500.0f, 1);
    
    leftnear = ProjM * ViewM * leftnear;
    leftnear = leftnear / leftnear.w;
    leftfar = ProjM * ViewM * leftfar;
    leftfar = leftfar / leftfar.w;
    rightnear = ProjM * ViewM * rightnear;
    rightnear = rightnear / rightnear.w;
    rightfar = ProjM * ViewM * rightfar;
    rightfar = rightfar / rightfar.w;
    
    setLineColour(vec3(0.0f, 0.0f, 0.0f));
    
    drawLine(vec2(leftnear.x, leftnear.y), vec2(leftfar.x, leftfar.y));
    drawLine(vec2(rightnear.x, rightnear.y), vec2(rightfar.x, rightfar.y));
    GLfloat verticesred[] =
    {
        leftnear.x, leftnear.y,   0,
        leftfar.x, leftfar.y,   0,
        rightnear.x, rightnear.y,   0,
        
        rightnear.x, rightnear.y,   0,
        rightfar.x, rightfar.y,   0,
        leftfar.x, leftfar.y,   0
    };
    glGenVertexArrays(1, &pl_vao);
    glUniform3f(col_uni, 0.5f, 0.5f, 0.5f);
    glBindVertexArray(pl_vao);
    //vertices
    glGenBuffers(1, &pl_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, pl_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 9 * 2, verticesred, GL_STATIC_DRAW);
    
    GLint posAttribxx = pl_shader.getAttribLocation("position");
    glEnableVertexAttribArray(posAttribxx);
    glVertexAttribPointer(posAttribxx, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glDrawArrays(GL_TRIANGLES, 0, 3 * 2);
    
    glDeleteBuffers(1, &pl_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    CHECK_GL_ERRORS;
    //paint grey color
//    setLineColour(vec3(0.5f, 0.5f, 0.5f));
//    for (int i = 1; i < 1000 ; i++) {
//        drawLine(vec2(leftnear.x + i * ((rightnear.x - leftnear.x)/1000), leftnear.y), vec2(leftfar.x + i * ((rightfar.x - leftfar.x)/1000), leftfar.y));
//    }
    //    cout << "road drawed" << endl;
    //    cout << "ln: " << leftnear.x << " " << leftnear.y << endl;
}
//----------------------------------------------------------------------------------------
/*
 * Draw Lane
 */
void A3::drawLanes(){
    std::vector<pair<glm::vec4, glm::vec4>> Lines;
    float width = 0.01f;
    for (float p = 0.0f; p > -1500.0f; p = p - 40.0f) {
        Lines.push_back({glm::vec4(-1.6667f, 0.0f, p + LaneFrame, 1), glm::vec4(-1.6667f, 0.0f, p - 15.0f + LaneFrame, 1)});
        Lines.push_back({glm::vec4(-1.6667f - width, 0.0f, p + LaneFrame, 1), glm::vec4(-1.6667f - width, 0.0f, p - 15.0f + LaneFrame, 1)});
        Lines.push_back({glm::vec4(-1.6667f + width, 0.0f, p + LaneFrame, 1), glm::vec4(-1.6667f + width, 0.0f, p - 15.0f + LaneFrame, 1)});
        Lines.push_back({glm::vec4(-1.6667f - 2*width, 0.0f, p + LaneFrame, 1), glm::vec4(-1.6667f - 2*width, 0.0f, p - 15.0f + LaneFrame, 1)});
        Lines.push_back({glm::vec4(-1.6667f + 2*width, 0.0f, p + LaneFrame, 1), glm::vec4(-1.6667f + 2*width, 0.0f, p - 15.0f + LaneFrame, 1)});
        Lines.push_back({glm::vec4(1.6667f, 0.0f, p + LaneFrame, 1), glm::vec4(1.6667f, 0.0f, p - 15.0f + LaneFrame, 1)});
        Lines.push_back({glm::vec4(1.6667f - width, 0.0f, p + LaneFrame, 1), glm::vec4(1.6667f - width, 0.0f, p - 15.0f + LaneFrame, 1)});
        Lines.push_back({glm::vec4(1.6667f + width, 0.0f, p + LaneFrame, 1), glm::vec4(1.6667f + width, 0.0f, p - 15.0f + LaneFrame, 1)});
        Lines.push_back({glm::vec4(1.6667f - 2*width, 0.0f, p + LaneFrame, 1), glm::vec4(1.6667f - 2*width, 0.0f, p - 15.0f + LaneFrame, 1)});
        Lines.push_back({glm::vec4(1.6667f + 2*width, 0.0f, p + LaneFrame, 1), glm::vec4(1.6667f + 2*width, 0.0f, p - 15.0f + LaneFrame, 1)});
    }
    setLineColour(vec3(1.0f, 1.0f, 1.0f));
    for (const auto & Line: Lines) {
        vec4 A = ProjM * ViewM * Line.first;
        A = A / A.w;
        vec4 B = ProjM * ViewM * Line.second;
        B = B / B.w;
        drawLine(vec2(A.x, A.y), vec2(B.x, B.y));
    }
    LaneFrame += (3.0f + speedup * (3.0f / 0.15f));
    if (LaneFrame > 40.0f) {
        LaneFrame = 0.0f;
    }
}
//----------------------------------------------------------------------------------------
/*
 * Draw The Power bar
 */
void A3::drawBar(){
    setLineColour(vec3(0.0f, 1.0f, 0.97f));
    drawLine(vec2(0.7f, 0.75f), vec2(0.7f, 0.8f));
    drawLine(vec2(0.7f, 0.75f), vec2(0.9f, 0.75f));
    drawLine(vec2(0.9f, 0.8f), vec2(0.9f, 0.75f));
    drawLine(vec2(0.9f, 0.8f), vec2(0.7f, 0.8f));
    float xside = 0.71f + (0.89f -0.71f)*((float)Power / 100.0f);
    //cout << xside << endl;
    //std::vector<pair<glm::vec2, glm::vec2>> Lines;
    for(int i = 0;i < 30;++i){
        float yboth = 0.76f + (0.79f - 0.76f)*((float)i / 30.0f);
        drawLine(vec2(0.71f, yboth), vec2(xside, yboth));
    }
}
//----------------------------------------------------------------------------------------

void A3::processLuaSceneFile(const std::string & filename) {
	// This version of the code treats the Lua file as an Asset,
	// so that you'd launch the program with just the filename
	// of a puppet in the Assets/ directory.
	std::string assetFilePath = getAssetFilePath(filename.c_str());
	m_rootNode = (SceneNode *)(import_lua(filename));

	// This version of the code treats the main program argument
	// as a straightforward pathname.
	//m_rootNode = std::shared_ptr<SceneNode>(import_lua(filename));
	if (!m_rootNode) {
		std::cerr << "Could not open " << filename << std::endl;
	}
}
//----------------------------------------------------------------------------------------
void A3::generateVertexBuffers()
{
    // Generate a vertex buffer to store line vertex positions
    {
        glGenBuffers(1, &m_vbo_positions);
        
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo_positions);
        
        // Set to GL_DYNAMIC_DRAW because the data store will be modified frequently.
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * kMaxVertices, nullptr,
                     GL_DYNAMIC_DRAW);
        
        
        // Unbind the target GL_ARRAY_BUFFER, now that we are finished using it.
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        CHECK_GL_ERRORS;
    }
    
    // Generate a vertex buffer to store line colors
    {
        glGenBuffers(1, &m_vbo_colours);
        
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo_colours);
        
        // Set to GL_DYNAMIC_DRAW because the data store will be modified frequently.
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * kMaxVertices, nullptr,
                     GL_DYNAMIC_DRAW);
        
        
        // Unbind the target GL_ARRAY_BUFFER, now that we are finished using it.
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        CHECK_GL_ERRORS;
    }
}
//----------------------------------------------------------------------------------------
void A3::createShaderProgram()
{
	m_shader.generateProgramObject();
	m_shader.attachVertexShader( getAssetFilePath("VertexShader.vs").c_str() );
	m_shader.attachFragmentShader( getAssetFilePath("FragmentShader.fs").c_str() );
	m_shader.link();

	m_shader_arcCircle.generateProgramObject();
	m_shader_arcCircle.attachVertexShader( getAssetFilePath("arc_VertexShader.vs").c_str() );
	m_shader_arcCircle.attachFragmentShader( getAssetFilePath("arc_FragmentShader.fs").c_str() );
	m_shader_arcCircle.link();
    
    A2m_shader.generateProgramObject();
    A2m_shader.attachVertexShader( getAssetFilePath("A2VertexShader.vs").c_str() );
    A2m_shader.attachFragmentShader( getAssetFilePath("A2FragmentShader.fs").c_str() );
    A2m_shader.link();
    
    
    pl_shader.generateProgramObject();
    pl_shader.attachVertexShader(getAssetFilePath( "pl_VertexShader.vs" ).c_str() );
    pl_shader.attachFragmentShader(getAssetFilePath( "pl_FragmentShader.fs" ).c_str() );
    pl_shader.link();
    
    tex_shader.generateProgramObject();
    tex_shader.attachVertexShader(getAssetFilePath( "texture.vs" ).c_str() );
    tex_shader.attachFragmentShader(getAssetFilePath( "texture.fs" ).c_str() );
    tex_shader.link();
    
    col_uni = pl_shader.getUniformLocation( "colour" );
}

//----------------------------------------------------------------------------------------
void A3::enableVertexShaderInputSlots()
{
	//-- Enable input slots for m_vao_meshData:
	{
		glBindVertexArray(m_vao_meshData);

		// Enable the vertex shader attribute location for "position" when rendering.
		m_positionAttribLocation = m_shader.getAttribLocation("position");
		glEnableVertexAttribArray(m_positionAttribLocation);

		// Enable the vertex shader attribute location for "normal" when rendering.
		m_normalAttribLocation = m_shader.getAttribLocation("normal");
		glEnableVertexAttribArray(m_normalAttribLocation);

		CHECK_GL_ERRORS;
	}


	//-- Enable input slots for m_vao_arcCircle:
	{
		glBindVertexArray(m_vao_arcCircle);

		// Enable the vertex shader attribute location for "position" when rendering.
		m_arc_positionAttribLocation = m_shader_arcCircle.getAttribLocation("position");
		glEnableVertexAttribArray(m_arc_positionAttribLocation);

		CHECK_GL_ERRORS;
	}
    
    {
        glBindVertexArray(m_vao);
        
        // Enable the attribute index location for "position" when rendering.
        GLint positionAttribLocation = A2m_shader.getAttribLocation( "position" );
        glEnableVertexAttribArray(positionAttribLocation);
        
        // Enable the attribute index location for "colour" when rendering.
        GLint colourAttribLocation = A2m_shader.getAttribLocation( "colour" );
        glEnableVertexAttribArray(colourAttribLocation);
        
        CHECK_GL_ERRORS;
    }

	// Restore defaults
	glBindVertexArray(0);
}

//----------------------------------------------------------------------------------------
void A3::uploadVertexDataToVbos (
		const MeshConsolidator & meshConsolidator
) {
	// Generate VBO to store all vertex position data
	{
		glGenBuffers(1, &m_vbo_vertexPositions);

		glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexPositions);

		glBufferData(GL_ARRAY_BUFFER, meshConsolidator.getNumVertexPositionBytes(),
				meshConsolidator.getVertexPositionDataPtr(), GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		CHECK_GL_ERRORS;
	}

	// Generate VBO to store all vertex normal data
	{
		glGenBuffers(1, &m_vbo_vertexNormals);

		glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexNormals);

		glBufferData(GL_ARRAY_BUFFER, meshConsolidator.getNumVertexNormalBytes(),
				meshConsolidator.getVertexNormalDataPtr(), GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		CHECK_GL_ERRORS;
	}

	// Generate VBO to store the trackball circle.
	{
		glGenBuffers( 1, &m_vbo_arcCircle );
		glBindBuffer( GL_ARRAY_BUFFER, m_vbo_arcCircle );

		float *pts = new float[ 2 * CIRCLE_PTS ];
		for( size_t idx = 0; idx < CIRCLE_PTS; ++idx ) {
			float ang = 2.0 * M_PI * float(idx) / CIRCLE_PTS;
			pts[2*idx] = cos( ang );
			pts[2*idx+1] = sin( ang );
		}

		glBufferData(GL_ARRAY_BUFFER, 2*CIRCLE_PTS*sizeof(float), pts, GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		CHECK_GL_ERRORS;
	}
}

//----------------------------------------------------------------------------------------
void A3::mapVboDataToVertexShaderInputLocations()
{
	// Bind VAO in order to record the data mapping.
	glBindVertexArray(m_vao_meshData);

	// Tell GL how to map data from the vertex buffer "m_vbo_vertexPositions" into the
	// "position" vertex attribute location for any bound vertex shader program.
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexPositions);
	glVertexAttribPointer(m_positionAttribLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	// Tell GL how to map data from the vertex buffer "m_vbo_vertexNormals" into the
	// "normal" vertex attribute location for any bound vertex shader program.
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexNormals);
	glVertexAttribPointer(m_normalAttribLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	//-- Unbind target, and restore default values:
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	CHECK_GL_ERRORS;

	// Bind VAO in order to record the data mapping.
	glBindVertexArray(m_vao_arcCircle);

	// Tell GL how to map data from the vertex buffer "m_vbo_arcCircle" into the
	// "position" vertex attribute location for any bound vertex shader program.
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo_arcCircle);
	glVertexAttribPointer(m_arc_positionAttribLocation, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

	//-- Unbind target, and restore default values:
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	CHECK_GL_ERRORS;
    
    // Bind VAO in order to record the data mapping.
    glBindVertexArray(m_vao);
    
    // Tell GL how to map data from the vertex buffer "m_vbo_positions" into the
    // "position" vertex attribute index for any bound shader program.
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_positions);
    GLint positionAttribLocation = A2m_shader.getAttribLocation( "position" );
    glVertexAttribPointer(positionAttribLocation, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    
    // Tell GL how to map data from the vertex buffer "m_vbo_colours" into the
    // "colour" vertex attribute index for any bound shader program.
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo_colours);
    GLint colorAttribLocation = A2m_shader.getAttribLocation( "colour" );
    glVertexAttribPointer(colorAttribLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    
    //-- Unbind target, and restore default values:
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    CHECK_GL_ERRORS;
}

//----------------------------------------------------------------------------------------
void A3::initPerspectiveMatrix()
{
    float aspect = ((float)m_windowWidth) / m_windowHeight;
    m_perpsective = glm::perspective(degreesToRadians(60.0f), aspect, 0.1f, 100.0f);
}


//----------------------------------------------------------------------------------------
void A3::initViewMatrix() {
    m_view = glm::lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, -1.0f),
                         vec3(0.0f, 1.0f, 0.0f));}

//----------------------------------------------------------------------------------------
void A3::initLightSources() {
	// World-space position
	m_light.position = vec3(-2.0f, 5.0f, 0.5f);
	m_light.rgbIntensity = vec3(0.8f); // White light
}

//----------------------------------------------------------------------------------------
void A3::uploadCommonSceneUniforms() {
	m_shader.enable();
	{
		//-- Set Perpsective matrix uniform for the scene:
		GLint location = m_shader.getUniformLocation("Perspective");
		glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(m_perpsective));
		CHECK_GL_ERRORS;
        
        location = m_shader.getUniformLocation("Pick");
        
        if(picking){
            glUniform1i(location, 1);
        }
        else{
            glUniform1i(location, 0);
		//-- Set LightSource uniform for the scene:
		{
			location = m_shader.getUniformLocation("light.position");
			glUniform3fv(location, 1, value_ptr(m_light.position));
			location = m_shader.getUniformLocation("light.rgbIntensity");
			glUniform3fv(location, 1, value_ptr(m_light.rgbIntensity));
			CHECK_GL_ERRORS;
		}

		//-- Set background light ambient intensity
		{
			location = m_shader.getUniformLocation("ambientIntensity");
			vec3 ambientIntensity(0.05f);
			glUniform3fv(location, 1, value_ptr(ambientIntensity));
			CHECK_GL_ERRORS;
		}
        }
	}
	m_shader.disable();
}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, before guiLogic().
 */
void A3::appLogic()
{
	uploadCommonSceneUniforms();
    
    // Place per frame, application logic here ...
    // Call at the beginning of frame, before drawing lines:
    initLineData();
 //   drawRoad();
    drawLanes();
//    draw the sky line
//    setLineColour(vec3(0.0f, 0.0f, 0.0f));
//    drawLine(vec2(-1.0f, -0.046f), vec2(1.0f, -0.046f));
}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, after appLogic(), but before the draw() method.
 */
void A3::guiLogic()
{
	if( !show_gui ) {
		return;
	}

	static bool firstRun(true);
	if (firstRun) {
		ImGui::SetNextWindowPos(ImVec2(50, 50));
		firstRun = false;
	}

	static bool showDebugWindow(true);
	ImGuiWindowFlags windowFlags(ImGuiWindowFlags_AlwaysAutoResize);
	float opacity(0.5f);

	ImGui::Begin("Properties", &showDebugWindow, ImVec2(100,100), opacity,
			windowFlags);


		// Add more gui elements here here ...
//    if (ImGui::BeginMenu("Application")) {
//        if (ImGui::Button("Reset Position (I)")) {
//            ResetPosition();
//        }
//        if (ImGui::Button("Reset Orientation (O)")) {
//            ResetOrientation();
//        }
//        if (ImGui::Button("Reset Joints (N)")) {
//            ResetJoints();
//        }
//        if (ImGui::Button("Reset All (A)")) {
//            ResetPosition();
//            ResetOrientation();
//            ResetJoints();
//        }
//        if (ImGui::Button("Quit (Q)")) {
//            glfwSetWindowShouldClose(m_window, GL_TRUE);
//        }
//        ImGui::EndMenu();
//    }
//    if (ImGui::BeginMenu("Edit")) {
//        if (ImGui::Button("Undo (U)")) {
//            //Undo();
//        }
//        if (ImGui::Button("Redo (R)")) {
//            //Redo();
//        }
//        ImGui::EndMenu();
//    }
//    if (ImGui::BeginMenu("Options")) {
//        if (ImGui::Checkbox("Circle (C)", &circle_mode)) {}
//        if (ImGui::Checkbox("Z-buffer (Z)", &zbuffer_mode)) {}
//        if (ImGui::Checkbox("Backface culling (B)", &backface_mode)) {}
//        if (ImGui::Checkbox("Frontface culling (F)", &frontface_mode)) {}
//        ImGui::EndMenu();
//    }
//    ImGui::PushID(0);
//    if (ImGui::RadioButton("##Position", &mode, 0)) {}
//    ImGui::PopID();
//    ImGui::SameLine();
//    ImGui::Text("Position/Orientation (P)");
//    
//    ImGui::PushID(1);
//    if (ImGui::RadioButton("##Joint", &mode, 1)) {}
//    ImGui::PopID();
//    ImGui::SameLine();
//    ImGui::Text("Joints (J)");
//
//
//		// Create Button, and check if it was clicked:
//        ImGui::SameLine();
    ImGui::Text( "HighScore: %d", HighScore);
    ImGui::Text( "Score: %d", Score);
    //ImGui::Text( "Power: %d", Power);
    if( ImGui::Button( "Quit Application" ) ) {
        glfwSetWindowShouldClose(m_window, GL_TRUE);
    }
//    ImGui::Text( "Framerate: %.1f FPS", ImGui::GetIO().Framerate );

	ImGui::End();
}

//----------------------------------------------------------------------------------------
// Update mesh specific shader uniforms:
static void updateShaderUniforms(
		const ShaderProgram & shader,
		const GeometryNode & node,
		const glm::mat4 & viewMatrix
) {

	shader.enable();
	{
		//-- Set ModelView matrix:
		GLint location = shader.getUniformLocation("ModelView");
		mat4 modelView = viewMatrix * node.trans;
		glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(modelView));
		CHECK_GL_ERRORS;

		//-- Set NormMatrix:
		location = shader.getUniformLocation("NormalMatrix");
		mat3 normalMatrix = glm::transpose(glm::inverse(mat3(modelView)));
		glUniformMatrix3fv(location, 1, GL_FALSE, value_ptr(normalMatrix));
		CHECK_GL_ERRORS;


		//-- Set Material values:
		location = shader.getUniformLocation("material.kd");
		vec3 kd = node.material.kd;
		glUniform3fv(location, 1, value_ptr(kd));
		CHECK_GL_ERRORS;
		location = shader.getUniformLocation("material.ks");
        vec3 ks = node.material.ks;
        glUniform3fv(location, 1, value_ptr(ks));
		CHECK_GL_ERRORS;
		location = shader.getUniformLocation("material.shininess");
		glUniform1f(location, node.material.shininess);
		CHECK_GL_ERRORS;

	}
	shader.disable();

}
//----------------------------------------------------------------------------------------
void A3::uploadVertexDataToVbos() {
    
    //-- Copy vertex position data into VBO, m_vbo_positions:
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo_positions);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vec2) * m_vertexData.numVertices,
                        m_vertexData.positions.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        CHECK_GL_ERRORS;
    }
    
    //-- Copy vertex colour data into VBO, m_vbo_colours:
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo_colours);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vec3) * m_vertexData.numVertices,
                        m_vertexData.colours.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        CHECK_GL_ERRORS;
    }
}
//----------------------------------------------------------------------------------------
/*
 * Called once per frame, after guiLogic().
 */
void A3::draw() {
    oldframe = !oldframe;
    ++Score;
    speedup += 0.0002f;
    if (Score > HighScore){
        HighScore = Score;
    }
    if(Power < 100){
        ++Power;
    }
    for (SceneNode *child : m_rootNode->children){
        if (oldframe){
            child->set_transform(translate(mat4(), vec3(0,0.005f,0)) * child->get_transform());
        }else{
            child->set_transform(translate(mat4(), vec3(0,-0.005f,0)) * child->get_transform());
        }
    }
    if (Rclicking && jumping <= 0) {
        mat4 T;
        T = translate(mat4(), vec3(0.08f, 0, 0));
        for (SceneNode *child : m_rootNode->children) {
            
            if (child->m_name == "torso" || child->m_name.substr(0,8) == "particle" ){
                //cout << child->get_transform() << endl;
                mat4 Temp = T * child->get_transform();
                if (Temp[3][0] > 1.4f) {
                    Temp[3][0] = 1.4f;
                }
                child->set_transform(Temp);
            }
        }
    }
    if (Lclicking && jumping <= 0) {
        mat4 T;
        T = translate(mat4(), vec3(-0.08f, 0, 0));
        for (SceneNode *child : m_rootNode->children) {
            if (child->m_name == "torso" || child->m_name.substr(0,8) == "particle"){
                mat4 Temp = T * child->get_transform();
                if (Temp[3][0] < -1.4f) {
                    Temp[3][0] = -1.4f;
                }
                child->set_transform(Temp);
            }
        }
    }
    if (jumping > 0.0f) {
        if (jumping + jumpspeed < 0.0f) {
            mat4 T;
            T = translate(mat4(), vec3(0, -jumping, 0));
            for (SceneNode *child : m_rootNode->children) {
                if (child->m_name == "torso" || child->m_name.substr(0,8) == "particle"){
                    child->set_transform(T * child->get_transform());
                }
            }
            jumping += jumpspeed;
            jumpspeed -= g;
            jumpspeedh = 0.0f;
            airf = 0.0f;
        }
        else{
            mat4 T;
            T = translate(mat4(), vec3(jumpspeedh, jumpspeed, 0));
            for (SceneNode *child : m_rootNode->children) {
                if (child->m_name == "torso" || child->m_name.substr(0,8) == "particle"){
                    mat4 Temp = T * child->get_transform();
                    if(Temp[3][0] < -1.4f && jumpspeedh < 0){
                        Temp[3][0] = -1.4f;
                    }
                    else if(Temp[3][0] > 1.4f && jumpspeedh > 0){
                        Temp[3][0] = 1.4f;
                    }
                    child->set_transform(Temp);
                }
            }
            jumping += jumpspeed;
            jumpspeed -= g;
            jumpspeedh += airf;
        }
    }
    float mycarX, mycarY, mycarZ;
    glClearColor(0.20, 0.40, 0.13, 1.0);
    for (SceneNode *child : m_rootNode->children) {
        mat4 T;
        T = translate(mat4(), vec3(0, 0, 0.15f + speedup));
        if (child->m_name == "torso"){
            mycarX = child->get_transform()[3][0];
            mycarY = child->get_transform()[3][1];
            mycarZ = child->get_transform()[3][2];
        }if (child->m_name != "torso"&& child->m_name.substr(0,8) != "particle"){
            child->set_transform(T * child->get_transform());
            //cout << child->get_transform()  << endl;
            mat4 Temp = child->get_transform();
            if (child->get_transform()[3][2] > 30.0f) {
                int randomInt = rand() % 3;
                //cout << randomInt << endl;
                if (randomInt == 0){
                    Temp[3][0] = 1.25f;
                    Temp[3][2] = -41.0f;
                }if (randomInt == 1) {
                    Temp[3][0] = 0.0f;
                    Temp[3][2] = -41.0f;
                }else if (randomInt == 2){
                    Temp[3][0] = -1.25f;
                    Temp[3][2] = -41.0f;
                }
                child->set_transform(Temp);
                if (child->m_name.substr(0,4) == "tree"){
                    mat4 Temp = child->get_transform();
                    int x = rand()%2;
                    Temp[3][0] = -6.0f;
                    Temp[3][2] = -62;
                    child->set_transform(Temp);
                }
                if (child->m_name.substr(0,4) == "trxx"){
                    mat4 Temp = child->get_transform();
                    int x = rand()%2;
                    Temp[3][0] = 6.0f;
                    Temp[3][2] = -62;
                    child->set_transform(Temp);
                }
                //cout << mycarX << " " << mycarY << " " << mycarZ << endl;
            }else if (Temp[3][0] < mycarX + 0.6 && Temp[3][0] > mycarX - 0.6 &&
                     Temp[3][2] < mycarZ + 1.2 && Temp[3][2] > mycarZ - 1.2 &&
                      mycarY < 0.3f){
                gameover = true;
                Score = 0;
                Power = 0;
            }
        }
    }
//    tex_shader.enable();
//    drawSky();
//    tex_shader.disable();
    pl_shader.enable();
    drawSky();
    drawRoad();
    if (Power < 100) {
        drawBar();
    }else{
        if (oldframe){
            drawBar();
        }
    }
    pl_shader.disable();
    
    uploadVertexDataToVbos();
    
    glBindVertexArray(m_vao);
    
    A2m_shader.enable();
    glDrawArrays(GL_LINES, 0, m_vertexData.numVertices);
    A2m_shader.disable();
    
    // Restore defaults
    glBindVertexArray(0);
    
    CHECK_GL_ERRORS;
    
    if(gameover){
        if (gameoverf == 0) {
            ISoundEngine* engine = createIrrKlangDevice();
            engine->play2D("Boom.mp3");
        }
        ++gameoverf;
        int i = rand()%3;
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f );
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        for(SceneNode *child : m_rootNode->children){
            if(child->m_name.substr(0,8) == "particle"){
                child->set_transform(Tarray[i] * child->get_transform());
                ++i;
            }
        }
    }
    if(gameoverf >= 50){
        gameover = false;gameoverf = 0;
        speedup = 0;
        for(SceneNode *child : m_rootNode->children){
            if(child->m_name.substr(0,8) == "particle"){
                mat4 Temp = child->get_transform();
                Temp[3][0] = mycarX;
                Temp[3][1] = mycarY;
                Temp[3][2] = mycarZ;
                child->set_transform(Temp);
            }
        }
    }
    
    if(zbuffer_mode){
        glEnable( GL_DEPTH_TEST );
    }
    if (backface_mode || frontface_mode) {
        glEnable(GL_CULL_FACE);
        if (backface_mode && frontface_mode) glCullFace(GL_FRONT_AND_BACK);
        else if (backface_mode) glCullFace(GL_BACK);
        else if (frontface_mode) glCullFace(GL_FRONT);
    }
	renderSceneGraph(*m_rootNode);
    if (!(backface_mode || frontface_mode)) {
        glDisable(GL_CULL_FACE);
    }
    if (zbuffer_mode){
        glDisable( GL_DEPTH_TEST );
    }
    if (circle_mode){
        renderArcCircle();
    }
    //glClearColor(0.0, 0.0, 0.0, 1.0);
//texture Mapping-------------------------------------------------------------------------
    
    
    // Two UV coordinatesfor each vertex. They were created with Blender. You'll learn shortly how to do this yourself.
//----------------------------------------------------------------------------------------
}
GLuint A3::loadBMP_custom(const char * imagepath){
    // Data read from the header of the BMP file
    unsigned char header[54]; // Each BMP file begins by a 54-bytes header
    unsigned int dataPos;     // Position in the file where the actual data begins
    unsigned int width, height;
    unsigned int imageSize;   // = width*height*3
    // Actual RGB data
    unsigned char * data;
    FILE * file = fopen(imagepath,"rb");
    if (!file){printf("Image could not be opened\n"); return 0;}
    if ( fread(header, 1, 54, file)!=54 ){ // If not 54 bytes read : problem
        printf("Not a correct BMP file\n");
        return false;
    }
    if ( header[0]!='B' || header[1]!='M' ){
        printf("Not a correct BMP file\n");
        return 0;
    }
    dataPos    = *(int*)&(header[0x0A]);
    imageSize  = *(int*)&(header[0x22]);
    width      = *(int*)&(header[0x12]);
    height     = *(int*)&(header[0x16]);
    // Some BMP files are misformatted, guess missing information
    if (imageSize==0)    imageSize=width*height*3; // 3 : one byte for each Red, Green and Blue component
    if (dataPos==0)      dataPos=54; // The BMP header is done that way
    // Create a buffer
    data = new unsigned char [imageSize];
    
    // Read the actual data from the file into the buffer
    fread(data,1,imageSize,file);
    
    //Everything is in memory now, the file can be closed
    fclose(file);
    // Create one OpenGL texture
    GLuint textureID;
    glGenTextures(1, &textureID);
    
    // "Bind" the newly created texture : all future texture functions will modify this texture
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    // Give the image to OpenGL
    glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, data);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    return(textureID);
}
//----------------------------------------------------------------------------------------
void A3::renderSceneGraph_helper(SceneNode *cur, const mat4 M) {
    //----------------------------------------
    mat4 m = cur->get_transform();
    cur->set_transform(M * m);
    
    for (SceneNode *child : cur->children) {
        child->parent = cur;
        renderSceneGraph_helper(child, cur->get_transform());
    }
    //----------------------------------------
    if (cur->m_nodeType == NodeType::GeometryNode && (!(gameover && cur->m_name.substr(0,8) != "particle"))) {
        const GeometryNode *geometryNode = static_cast<const GeometryNode *>(cur);
        if (picking) {
            int i = geometryNode->m_nodeId;
            int r = (i & 0x000000FF) >>  0;
            int g = (i & 0x0000FF00) >>  8;
            int b = (i & 0x00FF0000) >> 16;
            
            // OpenGL expects colors to be in [0,1], so divide by 255.
            m_shader.enable();
            GLuint id = m_shader.getUniformLocation("id");
            glUniform4f(id, r/255.0f, g/255.0f, b/255.0f, 1.0f);
            CHECK_GL_ERRORS;
            m_shader.disable();
        }else if (geometryNode->isSelected && mode == Joint) {
            m_shader.enable();
            GLuint Selected = m_shader.getUniformLocation("Selected");
            glUniform1i(Selected, 1);
            CHECK_GL_ERRORS;
            m_shader.disable();
        }
        
        updateShaderUniforms(m_shader, *geometryNode, m_view);
        
        // Get the BatchInfo corresponding to the GeometryNode's unique MeshId.
        BatchInfo batchInfo = m_batchInfoMap[geometryNode->meshId];
        
        //-- Now render the mesh:
        m_shader.enable();
        glDrawArrays(GL_TRIANGLES, batchInfo.startIndex, batchInfo.numIndices);
        
        GLuint Selected = m_shader.getUniformLocation("Selected");
        glUniform1i(Selected, 0);
        
        m_shader.disable();
    }
    
    cur->set_transform(m);
}
//----------------------------------------------------------------------------------------
void A3::renderSceneGraph(SceneNode & root) {

	// Bind the VAO once here, and reuse for all GeometryNode rendering below.
	glBindVertexArray(m_vao_meshData);

	// This is emphatically *not* how you should be drawing the scene graph in
	// your final implementation.  This is a non-hierarchical demonstration
	// in which we assume that there is a list of GeometryNodes living directly
	// underneath the root node, and that we can draw them in a loop.  It's
	// just enough to demonstrate how to get geometry and materials out of
	// a GeometryNode and onto the screen.

	// You'll want to turn this into recursive code that walks over the tree.
	// You can do that by putting a method in SceneNode, overridden in its
	// subclasses, that renders the subtree rooted at every node.  Or you
	// could put a set of mutually recursive functions in this class, which
	// walk down the tree from nodes of different types.
    
    renderSceneGraph_helper(&root, glm::mat4());

	glBindVertexArray(0);
	CHECK_GL_ERRORS;
}

//----------------------------------------------------------------------------------------
// Draw the trackball circle.
void A3::renderArcCircle() {
	glBindVertexArray(m_vao_arcCircle);

	m_shader_arcCircle.enable();
		GLint m_location = m_shader_arcCircle.getUniformLocation( "M" );
		float aspect = float(m_framebufferWidth)/float(m_framebufferHeight);
		glm::mat4 M;
		if( aspect > 1.0 ) {
			M = glm::scale( glm::mat4(), glm::vec3( 0.5/aspect, 0.5, 1.0 ) );
		} else {
			M = glm::scale( glm::mat4(), glm::vec3( 0.5, 0.5*aspect, 1.0 ) );
		}
		glUniformMatrix4fv( m_location, 1, GL_FALSE, value_ptr( M ) );
		glDrawArrays( GL_LINE_LOOP, 0, CIRCLE_PTS );
	m_shader_arcCircle.disable();

	glBindVertexArray(0);
	CHECK_GL_ERRORS;
}

//----------------------------------------------------------------------------------------
/*
 * Called once, after program is signaled to terminate.
 */
void A3::cleanup()
{

}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles cursor entering the window area events.
 */
bool A3::cursorEnterWindowEvent (
		int entered
) {
	bool eventHandled(false);

	// Fill in with event handling code...

	return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles mouse cursor movement events.
 */
bool A3::mouseMoveEvent (
		double xPos,
		double yPos
) {
	bool eventHandled(false);

    if (!ImGui::IsMouseHoveringAnyWindow()&&(Lclicking||Mclicking||Rclicking)) {
        int width;
        int height;
        glfwGetWindowSize(m_window, &width, &height);
        glm::vec2 curxy = glm::vec2((float)xPos, (float)yPos);
        double alpha = -((curxy.x - oldxy.x) / width) * PI / 5;
        float diameter;
        if (m_framebufferWidth < m_framebufferHeight) {
            diameter = m_framebufferWidth / 2;
        }else{
            diameter = m_framebufferHeight / 2;
        }
        mat4 T;
        if (Lclicking) {
            switch (mode) {
                case Position:
                    T = translate(mat4(), vec3((curxy.x - oldxy.x)/50000, -(curxy.y - oldxy.y)/50000, 0));
                    cout <<(curxy.x - oldxy.x)/50000 << endl;
                    TM *= T;
                    m_rootNode->set_transform(T * m_rootNode->get_transform());
                    break;
                case Joint:
                    break;
            }
        }
        if (Mclicking) {
            switch (mode) {
                case Position:
                    T = translate(mat4(), vec3(0, 0, (curxy.y - oldxy.y)/50000));
                    TM *= T;
                    m_rootNode->set_transform(T * m_rootNode->get_transform());
                    break;
                case Joint:
                    for (auto joint : Selected) {
                        float amount = curxy.y - oldxy.y;
                        if (joint->cur_x + amount > joint->m_joint_x.max) {
                            amount = joint->m_joint_x.max - (joint->cur_x);
                        }else if (joint->cur_x + amount < joint->m_joint_x.min){
                            amount = joint->m_joint_x.min - (joint->cur_x);
                        }
                        cout << "rotate" << endl;
                        joint->rotate('x', amount);
                        joint->cur_x += amount;
                    }
                    break;
            }
            
        }
        if (Rclicking) {
            float nx, ny, ox, oy, nz, oz, length, X, Y, Z, angle;
            switch (mode) {
                case Position:
                    nx = curxy.x * 2 / diameter;
                    ny = curxy.y * 2 / diameter;
                    ox = oldxy.x * 2 / diameter;
                    oy = oldxy.y * 2 / diameter;
                    
                    nz =1 - nx * nx - ny * ny;
                    oz =1 - ox * ox - oy * oy;
                    
                    length;
                    if (nz < 0) {
                        length = sqrt(1 - nz);
                        nz = 0;
                        nx = nx / length;
                        ny = ny / length;
                    }else{
                        nz = sqrt(nz);
                    }
                    if (oz < 0) {
                        length = sqrt(1 - oz);
                        oz = 0;
                        ox = ox / length;
                        oy = oy / length;
                    }else{
                        oz = sqrt(oz);
                    }
                    //cross product
                    X = (oy*nz - oz*ny);
                    Y = (oz*nx - ox*nz);
                    Z = (ox*ny - oy*nx);
                    //if the cross product is (0,0,0)
                    if (X == 0 && Y == 0 && Z == 0) {
                        T = mat4(vec4(1, 0, 0, 0),
                                 vec4(0, 1, 0, 0),
                                 vec4(0, 0, 1, 0),
                                 vec4(0, 0, 0, 1));
                    }else{
                        angle = sqrt(X*X + Y*Y + Z*Z);
                        X /= angle;
                        Y /= angle;
                        Z /= angle;
                        T = mat4(vec4(cos(angle) + X*X*(1-cos(angle)), X*Y*(1-cos(angle)) - Z*sin(angle), Z*X*(1-cos(angle)) + Y*sin(angle), 0),
                                 vec4(X*Y*(1-cos(angle)) + Z*sin(angle), cos(angle) + Y*Y*(1-cos(angle)), Z*Y*(1-cos(angle)) - X*sin(angle), 0),
                                 vec4(X*Z*(1-cos(angle)) - Y*sin(angle), Y*Z*(1-cos(angle)) + X*sin(angle), cos(angle) + Z*Z*(1-cos(angle)), 0),
                                 vec4(0, 0, 0, 1));
                    }
                    OM *= T;
                    m_rootNode->set_transform(m_rootNode->get_transform() * T);
                    break;
                case Joint:
                    for (auto joint : Selected) {
                        if (joint->m_name == "torso_head") {
                            float amount = curxy.x - oldxy.x;
                            if (joint->cur_y + amount > joint->m_joint_y.max) {
                                amount = joint->m_joint_y.max - (joint->cur_y);
                            }else if (joint->cur_y + amount < joint->m_joint_y.min){
                                amount = joint->m_joint_y.min - (joint->cur_y);
                            }
                            joint->rotate('y', amount);
                            joint->cur_y += amount;
                        }
                    }
                    break;
            }
        }
    }

	return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles mouse button events.
 */
bool A3::mouseButtonInputEvent (
		int button,
		int actions,
		int mods
) {
	bool eventHandled(false);

    if (!ImGui::IsMouseHoveringAnyWindow()) {
        if (actions == GLFW_PRESS) {
            double x, y;
            glfwGetCursorPos(m_window, &x, &y);
            oldxy = glm::vec2((float) x, (float) y);
            if (button == GLFW_MOUSE_BUTTON_LEFT){
                Lclicking = true;
                if (mode == Joint) {
                    picking = true;
                    m_rootNode->picking = true;
                    uploadCommonSceneUniforms();
                    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
                    
                    draw();
                    
                    x = x * (m_framebufferWidth / m_windowWidth);
                    y = m_windowHeight - y;
                    y = y * (m_framebufferHeight / m_windowHeight);
                    
                    ////////////////////////////////////////////
                    unsigned char data[4];
                    glReadPixels( int(x), int(y), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
                    int id = data[0] + data[1] * 256 + data[2] * 256 * 256;
                    ////////////////////////////////////////////
                    
                    cout << "color -> id: " << id <<endl;
                    
                    bool have = false;
                    if (m_rootNode->Map.find(id) != m_rootNode->Map.end()) {
                        have = true;
                    }
                    
                    if (have) {
                        SceneNode *node = m_rootNode->Map[id];
                        if (node->parent->m_nodeType == NodeType::JointNode){
                            JointNode *J = (JointNode *)node->parent;
                            node->isSelected = !(node->isSelected);
                            J->isSelected = !(J->isSelected);
                            if (J->isSelected) {
                                Selected.insert(J);
                            }else{
                                Selected.erase(J);
                            }
                        }
                    }
                    picking = false;
                    m_rootNode->picking = false;
                    CHECK_GL_ERRORS;
                }
            }if (button == GLFW_MOUSE_BUTTON_MIDDLE){
                if(jumping <= 0.0f && Power == 100){
                    Power = 0;
                    jumpspeed = 0.08f;
                    jumping = 0.0f;
                    mat4 T;
                    T = translate(mat4(), vec3(0, jumpspeed, 0));
                    TM *= T;
                    for (SceneNode *child : m_rootNode->children) {
                        if (child->m_name == "torso"){
                            child->set_transform(T * child->get_transform());
                            break;
                        }
                    }
                    jumping += jumpspeed;
                    jumpspeed -= g;
                    if (Lclicking) {
                        jumpspeedh = -0.08f;
                        airf = 0.0012f;
                    }
                    if (Rclicking) {
                        jumpspeedh = 0.08f;
                        airf = -0.0012f;
                    }
                }
                Mclicking = true;
            }if (button == GLFW_MOUSE_BUTTON_RIGHT){
                Rclicking = true;}
        }
        if (actions == GLFW_RELEASE) {
            if (button == GLFW_MOUSE_BUTTON_LEFT){
                Lclicking = false;
            }if (button == GLFW_MOUSE_BUTTON_MIDDLE){
                Mclicking = false;
            }if (button == GLFW_MOUSE_BUTTON_RIGHT){
                Rclicking = false;
            }
        }
    }
    eventHandled = true;

	return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles mouse scroll wheel events.
 */
bool A3::mouseScrollEvent (
		double xOffSet,
		double yOffSet
) {
	bool eventHandled(false);

	// Fill in with event handling code...

	return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles window resize events.
 */
bool A3::windowResizeEvent (
		int width,
		int height
) {
	bool eventHandled(false);
	initPerspectiveMatrix();
	return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles key input events.
 */
bool A3::keyInputEvent (
		int key,
		int action,
		int mods
) {
	bool eventHandled(false);

	if( action == GLFW_PRESS ) {
        if (key == GLFW_KEY_SPACE ) {
            if(jumping <= 0.0f && Power == 100){
                ISoundEngine* engine = createIrrKlangDevice();
                engine->play2D("jump.mp3");
                Power = 0;
                jumpspeed = 0.08f;
                jumping = 0.0f;
                mat4 T;
                T = translate(mat4(), vec3(0, jumpspeed, 0));
                TM *= T;
                for (SceneNode *child : m_rootNode->children) {
                    if (child->m_name == "torso"){
                        child->set_transform(T * child->get_transform());
                        break;
                    }
                }
                jumping += jumpspeed;
                jumpspeed -= g;
                if (Lclicking) {
                    jumpspeedh = -0.08f;
                    airf = 0.0012f;
                }
                if (Rclicking) {
                    jumpspeedh = 0.08f;
                    airf = -0.0012f;
                }
            }
        }
		if (key == GLFW_KEY_M ) {
			show_gui = !show_gui;
			eventHandled = true;
		}
        else if (key == GLFW_KEY_P){
            mode = Position;
        }
        else if (key == GLFW_KEY_J){
            mode = Joint;
        }
        else if (key == GLFW_KEY_Q){
            glfwSetWindowShouldClose(m_window, GL_TRUE);
        }
        else if (key == GLFW_KEY_Z){
            zbuffer_mode = !zbuffer_mode;
        }
        else if (key == GLFW_KEY_C){
            circle_mode = !circle_mode;
        }
        else if (key == GLFW_KEY_F){
            frontface_mode = !frontface_mode;
        }
        else if (key == GLFW_KEY_B){
            backface_mode = !backface_mode;
        }
        else if (key == GLFW_KEY_I){
            ResetPosition();
        }
        else if (key == GLFW_KEY_O){
            ResetOrientation();
        }
        else if (key == GLFW_KEY_N){
            ResetJoints();
        }
        else if (key == GLFW_KEY_A){
            ResetPosition();
            ResetOrientation();
            ResetJoints();
        }
	}
	// Fill in with event handling code...

	return eventHandled;
}
//----------------------------------------------------------------------------------------
void A3::ResetPosition() {
    m_rootNode->set_transform(inverse(TM) * m_rootNode->get_transform());
    TM = mat4();
}
void A3::ResetOrientation() {
    m_rootNode->set_transform(m_rootNode->get_transform() * inverse(OM));
    OM = mat4();
}
void A3::ResetJoints(){
    for (int i = 0; i < m_rootNode->nodeInstanceCount; ++i) {
        
        if(m_rootNode->Map[i]->m_nodeType == NodeType::JointNode){
            JointNode *joint = static_cast<JointNode *>(m_rootNode->Map[i]);
            if (joint->cur_x != joint->m_joint_x.init) {
                cout << joint->m_joint_x.init << endl;
                cout << joint->cur_x << endl;
                float init = joint->m_joint_x.init;
                float cur_x = joint->cur_x;
                joint->rotate('x', init - cur_x);
                joint->cur_x = init;
            }
        }
        if(m_rootNode->Map[i]->m_nodeType == NodeType::JointNode){
            JointNode *joint = static_cast<JointNode *>(m_rootNode->Map[i]);
            if (joint->cur_y != joint->m_joint_y.init) {
                cout << joint->m_joint_y.init << endl;
                cout << joint->cur_y << endl;
                float init = joint->m_joint_y.init;
                float cur_y = joint->cur_y;
                joint->rotate('y', init - cur_y);
                joint->cur_y = init;
            }
        }
    }
}