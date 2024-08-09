// Chess Engine
#include "../src/bitboard/bit_board.h"

// Misc Imports
#include <iostream>
#include <random>
#include <fstream>
#include <sstream>
#include <thread>
#include <future>


// My OpenGL Engine
#include "include/OpenGLEngine/Renderer.h"
#include "include/OpenGLEngine/VertexBuffer.h"
#include "include/OpenGLEngine/IndexBuffer.h"
#include "include/OpenGLEngine/VertexArray.h"  
#include "include/OpenGLEngine/Shader.h"
#include "include/OpenGLEngine/VertexBufferLayout.h"
#include "include/OpenGLEngine/Texture.h"
#include <GL/glew.h>
#include <GL/glut.h>
#include <GLFW/glfw3.h>

// glm for matrix transformations
#include "include/glm/glm.hpp"
#include "include/glm/gtc/matrix_transform.hpp"

// ImGui for debugging
#include "include/imgui/imgui.h"
#include "include/imgui/imgui_impl_glfw_gl3.h"

// ChessGUI
#include "ChessGUI.h"

struct BotResult{
    int move;
    float evaluation;
    bool processed = false;
};

BotResult bot_callback(BitBoard board, int depth, bool quien);

int main() {

    GLFWwindow* window;
    int count;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    
    std::cout << "Number of monitors: " << count << std::endl;
    int width_mm, height_mm;
    // glfwGetMonitorPhysicalSize(primary, &width_mm, &height_mm);

    // std::cout << "Width: " << width_mm << "mm, Height: " << height_mm << "mm" << std::endl;

    /* Initialize the library */
    if (!glfwInit())
        return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // Set the major version of OpenGL
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); // Set the minor version of OpenGL
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // Set the profile of OpenGL to core (No backwards compatibility)

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(1920, 1080, "Lar Chess", NULL, NULL);
    glfwSetWindowAspectRatio(window, 19, 10);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    glfwSwapInterval(1); // Enable VSync

    // Initialize GLEW only after creating the window
    if (glewInit() != GLEW_OK) 
        std::cout << "Error!" << std::endl;

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;

    GLCall(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)); // Set the blending function
    GLCall(glEnable(GL_BLEND)); // Enable blending

    Renderer renderer; // Instantiate a renderer

    ImGui::CreateContext();

    ImGui_ImplGlfwGL3_Init(window, true);

    ImGui::StyleColorsDark();

    ChessGUI chessGUI(window);

    BitBoard board;

    board.parse_fen("6k2/4Q3/6K2/8/8/8/8/8 b - 0 22");
    bool human_vs_human = false;
    int human_player = 0;
    int depth = 4;
    BotResult result;
    bool processing_flag = false;
    std::future<BotResult> future;
    board.parse_fen(start_position);
    chessGUI.set_board(board.bitboard_to_board());
    
    // /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {   
        /* Render here */
        GLCall(glClearColor(0.0f, 0.0f, 0.0f, 1.0f)); // Set the clear color

        // Get the current window size
        renderer.Clear(); // Clear the color buffer
        ImGui_ImplGlfwGL3_NewFrame();

        // std::cout << "1.Render Cleared" << std::endl;

        chessGUI.OnUpdate(0.0f);

        // std::cout << "2.OnUpdate" << std::endl;
        chessGUI.OnRender();

        // std::cout << "3.OnRender" << std::endl;
        ImGui::Begin("Test");

        chessGUI.OnImGuiRender();
        ImGui::End();

        ImGui::Render();
        ImGui_ImplGlfwGL3_RenderDrawData(ImGui::GetDrawData());

        // std::cout << "4.ImGUI" << std::endl;

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();

        // std::cout << "5.PollEvents" << std::endl;
        // std::cout << "Engine" << std::endl;

        if (human_vs_human){
            // std::cout << "Quering the GUI" << std::endl;
            std::string gui_move = chessGUI.get_player_move();
            // std::cout << "GUI move" << gui_move << std::endl;
            if (gui_move != ""){
                board.make_player_move(gui_move.c_str());
                chessGUI.set_board(board.bitboard_to_board());
            }
        }else if (board.get_side() == human_player){
            std::string gui_move = chessGUI.get_player_move();
            if (gui_move != ""){
                board.make_player_move(gui_move.c_str());   
                chessGUI.set_board(board.bitboard_to_board());
            }
        }else if (board.get_side() == !human_player || human_player == -1){
            // std::thread bt(bot_callback, std::ref(result), std::ref(board), std::ref(depth), std::ref(human_player));

            if (!processing_flag){
                future = std::async(std::launch::async, bot_callback, board, depth, true);
                processing_flag = true;
                chessGUI.lock_board();
            }

            if (future.wait_for(std::chrono::milliseconds(10)) == std::future_status::ready) {
                // std::cout << "Future is ready" << std::endl;
                result = future.get();
                // std::cout << "Result: " << result.move << std::endl;
                // std::cout << "Processed: " << result.processed << std::endl;
            } 

            if (result.processed){
                std::cout << "Bot Move: " << board.move_to_uci(result.move) << " Evaluation: " << result.evaluation << std::endl;
                board.make_bot_move(result.move);
                chessGUI.set_board(board.bitboard_to_board());
                chessGUI.unlock_board();
                processing_flag = false;
                result.move = 0;
                result.evaluation = 0.0;
                result.processed = false;
            }
        
        }

        // if (board.is_checkmate()){
        //     break;
        // }

        // std::cout << "6.Make Move" << std::endl;
        //  std::cout << "----------------------------------------------------------" << std::endl;
    }

     std::cout << "7.End Program" << std::endl;
    ImGui_ImplGlfwGL3_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();

}

BotResult bot_callback(BitBoard board, int depth, bool quien){
    BotResult result;
    int bot_move;
    // std::cout << "Inside thread" << std::endl;
    int start = get_time_ms();
    board.reset_leaf_nodes();
    float eval = board.alpha_beta(depth, -1000000, 1000000, quien);
    int end = get_time_ms();
    long nodes = board.get_leaf_nodes();
    std::cout << "-----------------------------------" << std::endl;
    std::cout << "Time taken: " << end - start << "ms - Visited " << nodes << " nodes"  << std::endl;
    // std::cout << "Minmax calculated" << std::endl;
    result.move = board.get_bot_best_move();
    result.evaluation = eval;
    result.processed = true;

    while (get_time_ms() - start < 500){
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return result;
}

