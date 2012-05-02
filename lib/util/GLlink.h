#ifndef __GLLINK_H__
#define __GLLINK_H__

#include <string>
#include <vector>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#include <hrpCorba/ModelLoader.hh>
#include <hrpUtil/Eigen3d.h>

class GLcamera;

class GLlink
{
public:
    typedef enum {FREE_JOINT, 
                  FIXED_JOINT, 
                  ROTATIONAL_JOINT, 
                  SLIDE_JOINT} JointType;

    GLlink(const OpenHRP::LinkInfo &i_li, OpenHRP::BodyInfo_var i_binfo);
    ~GLlink();
    void draw();
    void setParent(GLlink *i_parent);
    void addChild(GLlink *i_child);
    void setQ(double i_q);
    void setTransform(double i_trans[16]);
    double *getTransform() { return m_trans; } 
    int jointId();
    const std::string& name() { return m_name; }

    GLcamera *findCamera(const char *i_name);

    void computeAbsTransform(double o_trans[16]);
    void setAbsTransform(double o_trans[16]);
    JointType jointType() { return m_jointType; }
    static void useAbsTransformToDraw();

private:
    static bool m_useAbsTransformToDraw;
    GLlink *m_parent;
    std::string m_name;
    std::vector<GLlink *> m_children;
    std::vector<GLcamera *> m_cameras;
    hrp::Vector3 m_axis;
    double m_trans[16], m_T_j[16], m_absTrans[16];
    int m_list, m_jointId;
    JointType m_jointType;
    std::vector<GLuint> m_textures;
};
#endif