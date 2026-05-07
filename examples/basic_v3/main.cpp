#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <map>

#include "shaders.h"
#include "window.h"

#define TINYGLTF3_IMPLEMENTATION
#define TINYGLTF3_ENABLE_FS
#define TINYGLTF3_ENABLE_STB_IMAGE
#define TINYGLTF3_ENABLE_STB_IMAGE_WRITE
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#define JSON_NOEXCEPTION
#include "../../tiny_gltf_v3.h"

#define BUFFER_OFFSET(i) ((char*)NULL + (i))

bool loadModel(tg3_model& model, const char* filename) {
    tg3_error_stack err_stack;
    tg3_parse_options opts;
    tg3_parse_options_init(&opts);
    tg3_error_stack_init(&err_stack);

    auto err_code =
        tg3_parse_file(&model, &err_stack, filename, strlen(filename), &opts);
    if (err_code != TG3_OK) {
        for (int i = 0; i < err_stack.count; i++)
            fprintf(stderr, "[%s] %s\n",
                    tg3_severity_str(err_stack.entries[i].severity),
                    err_stack.entries[i].message);
    }

    tg3_error_stack_free(&err_stack);
    return (err_code == TG3_OK);
}

void bindMesh(std::map<int, GLuint>& vbos, tg3_model& model,
              const tg3_mesh& mesh) {
    for (size_t i = 0; i < model.buffer_views_count; ++i) {
        const auto& bufferView = model.buffer_views[i];
        if (bufferView.target == 0) {  // TODO impl drawarrays
            std::cout << "WARN: bufferView.target is zero" << std::endl;
            continue;
            // clang-format off
      // Unsupported bufferView.
      /*
        From spec2.0 readme:
        https://github.com/KhronosGroup/glTF/tree/master/specification/2.0
                 ... drawArrays function should be used with a count equal to
        the count            property of any of the accessors referenced by the
        attributes            property            (they are all equal for a
        given            primitive).
      */
            // clang-format on
        }

        const auto& buffer = model.buffers[bufferView.buffer];
        std::cout << "bufferview.target " << bufferView.target << std::endl;

        GLuint vbo;
        glGenBuffers(1, &vbo);
        vbos[i] = vbo;
        glBindBuffer(bufferView.target, vbo);

        std::cout << "buffer.data.size = " << buffer.data.count
                  << ", bufferview.byteOffset = " << bufferView.byte_offset
                  << std::endl;

        glBufferData(bufferView.target, bufferView.byte_length,
                     &buffer.data.data[0] + bufferView.byte_offset,
                     GL_STATIC_DRAW);
    }

    for (size_t i = 0; i < mesh.primitives_count; ++i) {
        tg3_primitive primitive = mesh.primitives[i];
        tg3_accessor indexAccessor = model.accessors[primitive.indices];

        for (size_t i = 0; i < primitive.attributes_count; ++i) {
            const auto& attrib = primitive.attributes[i];
            tg3_accessor accessor = model.accessors[attrib.value];
            int byteStride = tg3_accessor_byte_stride(
                &accessor, &model.buffer_views[accessor.buffer_view]);
            glBindBuffer(GL_ARRAY_BUFFER, vbos[accessor.buffer_view]);

            int size = 1;
            if (accessor.type != TG3_TYPE_SCALAR) {
                size = accessor.type;
            }

            int vaa = -1;
            if (tg3_str_equals_cstr(attrib.key, "POSITION")) vaa = 0;
            if (tg3_str_equals_cstr(attrib.key, "NORMAL")) vaa = 1;
            if (tg3_str_equals_cstr(attrib.key, "TEXCOORD_0")) vaa = 2;
            if (vaa > -1) {
                glEnableVertexAttribArray(vaa);
                glVertexAttribPointer(vaa, size, accessor.component_type,
                                      accessor.normalized ? GL_TRUE : GL_FALSE,
                                      byteStride,
                                      BUFFER_OFFSET(accessor.byte_offset));
            } else
                std::cout << "vaa missing: " << attrib.key.data << std::endl;
        }

        if (model.textures_count > 0) {
            // fixme: Use material's baseColor
            const tg3_texture* tex = &model.textures[0];

            if (tex->source > -1) {
                GLuint texid;
                glGenTextures(1, &texid);

                const tg3_image& image = model.images[tex->source];

                glBindTexture(GL_TEXTURE_2D, texid);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                GL_LINEAR);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

                GLenum format = GL_RGBA;

                if (image.component == 1) {
                    format = GL_RED;
                } else if (image.component == 2) {
                    format = GL_RG;
                } else if (image.component == 3) {
                    format = GL_RGB;
                } else {
                    // ???
                }

                GLenum type = GL_UNSIGNED_BYTE;
                if (image.bits == 8) {
                    // ok
                } else if (image.bits == 16) {
                    type = GL_UNSIGNED_SHORT;
                } else {
                    // ???
                }

                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width,
                             image.height, 0, format, type,
                             &image.image.data[0]);
            }
        }
    }
}

// bind models
void bindModelNodes(std::map<int, GLuint>& vbos, tg3_model& model,
                    const tg3_node& node) {
    if ((node.mesh >= 0) && (node.mesh < model.meshes_count)) {
        bindMesh(vbos, model, model.meshes[node.mesh]);
    }

    for (size_t i = 0; i < node.children_count; i++) {
        assert((node.children[i] >= 0) && (i < model.nodes_count));
        bindModelNodes(vbos, model, model.nodes[node.children[i]]);
    }
}

std::pair<GLuint, std::map<int, GLuint> > bindModel(tg3_model& model) {
    std::map<int, GLuint> vbos;
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    const tg3_scene& scene = model.scenes[model.default_scene];
    for (size_t i = 0; i < scene.nodes_count; ++i) {
        assert((scene.nodes[i] >= 0) && (scene.nodes[i] < model.nodes_count));
        bindModelNodes(vbos, model, model.nodes[scene.nodes[i]]);
    }

    glBindVertexArray(0);
    // cleanup vbos but do not delete index buffers yet
    for (auto it = vbos.cbegin(); it != vbos.cend();) {
        tg3_buffer_view bufferView = model.buffer_views[it->first];
        if (bufferView.target != GL_ELEMENT_ARRAY_BUFFER) {
            glDeleteBuffers(1, &vbos[it->first]);
            vbos.erase(it++);
        } else {
            ++it;
        }
    }

    return {vao, vbos};
}

void drawMesh(const std::map<int, GLuint>& vbos, tg3_model& model,
              const tg3_mesh& mesh) {
    for (size_t i = 0; i < mesh.primitives_count; ++i) {
        tg3_primitive primitive = mesh.primitives[i];
        tg3_accessor indexAccessor = model.accessors[primitive.indices];

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                     vbos.at(indexAccessor.buffer_view));

        glDrawElements(primitive.mode, indexAccessor.count,
                       indexAccessor.component_type,
                       BUFFER_OFFSET(indexAccessor.byte_offset));
    }
}

// recursively draw node and children nodes of model
void drawModelNodes(const std::pair<GLuint, std::map<int, GLuint> >& vaoAndEbos,
                    tg3_model& model, const tg3_node& node) {
    if ((node.mesh >= 0) && (node.mesh < model.meshes_count)) {
        drawMesh(vaoAndEbos.second, model, model.meshes[node.mesh]);
    }
    for (size_t i = 0; i < node.children_count; i++) {
        drawModelNodes(vaoAndEbos, model, model.nodes[node.children[i]]);
    }
}

void drawModel(const std::pair<GLuint, std::map<int, GLuint> >& vaoAndEbos,
               tg3_model& model) {
    glBindVertexArray(vaoAndEbos.first);

    const tg3_scene& scene = model.scenes[model.default_scene];
    for (size_t i = 0; i < scene.nodes_count; ++i) {
        drawModelNodes(vaoAndEbos, model, model.nodes[scene.nodes[i]]);
    }

    glBindVertexArray(0);
}

void dbgModel(tg3_model& model) {
    for (size_t i = 0; i < model.meshes_count; ++i) {
        const auto& mesh = model.meshes[i];
        std::cout << "mesh : " << mesh.name.data << std::endl;
        for (size_t i = 0; i < mesh.primitives_count; ++i) {
            const tg3_primitive& primitive = mesh.primitives[i];
            const tg3_accessor& indexAccessor =
                model.accessors[primitive.indices];

            std::cout << "indexaccessor: count " << indexAccessor.count
                      << ", type " << indexAccessor.component_type << std::endl;

            const tg3_material& mat = model.materials[primitive.material];
            // for (auto& mats : mat.values) {
            //   std::cout << "mat : " << mats.first.c_str() << std::endl;
            // }

            for (size_t i = 0; i < model.images_count; ++i) {
                const auto& image = model.images[i];
                std::cout << "image name : " << image.uri.data << std::endl;
                std::cout << "  size : " << image.image.count << std::endl;
                std::cout << "  w/h : " << image.width << "/" << image.height
                          << std::endl;
            }

            std::cout << "indices : " << primitive.indices << std::endl;
            std::cout << "mode     : "
                      << "(" << primitive.mode << ")" << std::endl;

            // for (auto& attrib : primitive.attributes) {
            //   std::cout << "attribute : " << attrib.first.c_str() <<
            //   std::endl;
            // }
        }
    }
}

glm::mat4 genView(glm::vec3 pos, glm::vec3 lookat) {
    // Camera matrix
    glm::mat4 view = glm::lookAt(
        pos,                // Camera in World Space
        lookat,             // and looks at the origin
        glm::vec3(0, 1, 0)  // Head is up (set to 0,-1,0 to look upside-down)
    );

    return view;
}

glm::mat4 genMVP(glm::mat4 view_mat, glm::mat4 model_mat, float fov, int w,
                 int h) {
    glm::mat4 Projection = glm::perspective(
        glm::radians(fov), (float)w / (float)h, 0.01f, 1000.0f);

    // Or, for an ortho camera :
    // glm::mat4 Projection = glm::ortho(-10.0f,10.0f,-10.0f,10.0f,0.0f,100.0f);
    // // In world coordinates

    glm::mat4 mvp = Projection * view_mat * model_mat;

    return mvp;
}

void displayLoop(Window& window, const std::string& filename) {
    Shaders shader = Shaders();
    glUseProgram(shader.pid);

    // grab uniforms to modify
    GLuint MVP_u = glGetUniformLocation(shader.pid, "MVP");
    GLuint sun_position_u = glGetUniformLocation(shader.pid, "sun_position");
    GLuint sun_color_u = glGetUniformLocation(shader.pid, "sun_color");

    tg3_model model;
    if (!loadModel(model, filename.c_str())) return;

    std::pair<GLuint, std::map<int, GLuint> > vaoAndEbos = bindModel(model);
    // dbgModel(model); return;

    // Model matrix : an identity matrix (model will be at the origin)
    glm::mat4 model_mat = glm::mat4(1.0f);
    glm::mat4 model_rot = glm::mat4(1.0f);
    glm::vec3 model_pos = glm::vec3(-3, 0, -3);

    // generate a camera view, based on eye-position and lookAt world-position
    glm::mat4 view_mat = genView(glm::vec3(2, 2, 20), model_pos);

    glm::vec3 sun_position = glm::vec3(3.0, 10.0, -5.0);
    glm::vec3 sun_color = glm::vec3(1.0);

    while (!window.Close()) {
        window.Resize();

        glClearColor(0.2, 0.2, 0.2, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 trans =
            glm::translate(glm::mat4(1.0f), model_pos);  // reposition model
        model_rot = glm::rotate(model_rot, glm::radians(0.8f),
                                glm::vec3(0, 1, 0));  // rotate model on y axis
        model_mat = trans * model_rot;

        // build a model-view-projection
        GLint w, h;
        glfwGetWindowSize(window.window, &w, &h);
        glm::mat4 mvp = genMVP(view_mat, model_mat, 45.0f, w, h);
        glUniformMatrix4fv(MVP_u, 1, GL_FALSE, &mvp[0][0]);

        glUniform3fv(sun_position_u, 1, &sun_position[0]);
        glUniform3fv(sun_color_u, 1, &sun_color[0]);

        drawModel(vaoAndEbos, model);
        glfwSwapBuffers(window.window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &vaoAndEbos.first);
    tg3_model_free(&model);
}

static void error_callback(int error, const char* description) {
    (void)error;
    fprintf(stderr, "Error: %s\n", description);
}

int main(int argc, char** argv) {
    std::string filename = "../../../models/Cube/Cube.gltf";

    if (argc > 1) {
        filename = argv[1];
    }

    glfwSetErrorCallback(error_callback);

    if (!glfwInit()) return -1;

    // Force create OpenGL 3.3
    // NOTE(syoyo): Linux + NVIDIA driver segfaults for some reason? commenting
    // out glfwWindowHint will work. Note (PE): On laptops with intel hd
    // graphics card you can overcome the segfault by enabling experimental, see
    // below (tested on lenovo thinkpad)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glewExperimental = GL_TRUE;

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    Window window = Window(800, 600, "TinyGLTF basic example");
    glfwMakeContextCurrent(window.window);

#ifdef __APPLE__
    // https://stackoverflow.com/questions/50192625/openggl-segmentation-fault
    glewExperimental = GL_TRUE;
#endif

    glewInit();
    std::cout << glGetString(GL_RENDERER) << ", " << glGetString(GL_VERSION)
              << std::endl;

    if (!GLEW_VERSION_3_3) {
        std::cerr << "OpenGL 3.3 is required to execute this app." << std::endl;
        return EXIT_FAILURE;
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

    displayLoop(window, filename);

    glfwTerminate();
    return 0;
}
