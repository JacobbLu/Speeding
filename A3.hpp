#pragma once

#include "cs488-framework/CS488Window.hpp"
#include "cs488-framework/OpenGLImport.hpp"
#include "cs488-framework/ShaderProgram.hpp"
#include "cs488-framework/MeshConsolidator.hpp"

#include "SceneNode.hpp"
#include "JointNode.hpp"

#include <glm/glm.hpp>
#include <memory>
#include <unordered_set>
#include <vector>
#include <irrKlang.h>

struct LightSource {
	glm::vec3 position;
	glm::vec3 rgbIntensity;
};

// Set a global maximum number of vertices in order to pre-allocate VBO data
// in one shot, rather than reallocating each frame.
const GLsizei kMaxVertices = 10000;


// Convenience class for storing vertex data in CPU memory.
// Data should be copied over to GPU memory via VBO storage before rendering.
class VertexData {
public:
    VertexData();
    
    std::vector<glm::vec2> positions;
    std::vector<glm::vec3> colours;
    GLuint index;
    GLsizei numVertices;
};


class A3 : public CS488Window {
public:
	A3(const std::string & luaSceneFile);
	virtual ~A3();

protected:
    void generateVertexBuffers();
	virtual void init() override;
	virtual void appLogic() override;
	virtual void guiLogic() override;
	virtual void draw() override;
	virtual void cleanup() override;

	//-- Virtual callback methods
	virtual bool cursorEnterWindowEvent(int entered) override;
	virtual bool mouseMoveEvent(double xPos, double yPos) override;
	virtual bool mouseButtonInputEvent(int button, int actions, int mods) override;
	virtual bool mouseScrollEvent(double xOffSet, double yOffSet) override;
	virtual bool windowResizeEvent(int width, int height) override;
	virtual bool keyInputEvent(int key, int action, int mods) override;

	//-- One time initialization methods:
	void processLuaSceneFile(const std::string & filename);
	void createShaderProgram();
	void enableVertexShaderInputSlots();
	void uploadVertexDataToVbos(const MeshConsolidator & meshConsolidator);
	void mapVboDataToVertexShaderInputLocations();
	void initViewMatrix();
	void initLightSources();

	void initPerspectiveMatrix();
	void uploadCommonSceneUniforms();
    void renderSceneGraph_helper(SceneNode *cur, const glm::mat4 M);
	void renderSceneGraph(SceneNode &node);
	void renderArcCircle();
    void ResetPosition();
    void ResetOrientation();
    void ResetJoints();
    
    void uploadVertexDataToVbos();

	glm::mat4 m_perpsective;
	glm::mat4 m_view;

	LightSource m_light;

	//-- GL resources for mesh geometry data:
	GLuint m_vao_meshData;
	GLuint m_vbo_vertexPositions;
	GLuint m_vbo_vertexNormals;
	GLint m_positionAttribLocation;
	GLint m_normalAttribLocation;
	ShaderProgram m_shader;
    ShaderProgram A2m_shader;
    ShaderProgram pl_shader;
    GLint col_uni;
    GLuint pl_vao; // Vertex Array Object
    GLuint pl_vbo; // Vertex Buffer Object
    ShaderProgram tex_shader;

	//-- GL resources for trackball circle geometry:
	GLuint m_vbo_arcCircle;
	GLuint m_vao_arcCircle;
	GLint m_arc_positionAttribLocation;
	ShaderProgram m_shader_arcCircle;
    
    GLuint m_vao;            // Vertex Array Object
    GLuint m_vbo_positions;  // Vertex Buffer Object
    GLuint m_vbo_colours;    // Vertex Buffer Object

	// BatchInfoMap is an associative container that maps a unique MeshId to a BatchInfo
	// object. Each BatchInfo object contains an index offset and the number of indices
	// required to render the mesh with identifier MeshId.
	BatchInfoMap m_batchInfoMap;

	std::string m_luaSceneFile;

	SceneNode *m_rootNode;
    //------------------------------------------------------------------------------------
    int mode;
    std::unordered_set<JointNode*> Selected;
    
    //----------------------------------------------------------------------------------------
    float Far;
    float Near;
    float Aspect_ratio;
    float F;
    float LaneFrame;
    //----------------------------------------------------------------------------------------
    glm::mat4 ViewM;
    glm::mat4 ProjM;
    //----------------------------------------------------------------------------------------
    VertexData m_vertexData;
    
    glm::vec3 m_currentLineColour;
    
    void initLineData();
    
    void setLineColour(const glm::vec3 & colour);
    
    void drawLine (
                   const glm::vec2 & v0,
                   const glm::vec2 & v1
                   );
    void drawSky();
    void drawRoad ();
    void drawLanes ();
    void drawBar();
    
    GLuint loadBMP_custom(const char * imagepath);
};
