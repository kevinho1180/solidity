#include <Compiler.h>
#include <ui/UserInterface.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <imgui/imgui.h>

#include <imgui/backends/imgui_impl_opengl3.h>
#include <imgui/backends/imgui_impl_sdl2.h>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include <iostream>
#include <string>
#include <vector>

namespace po = boost::program_options;
namespace bf = boost::filesystem;

int main(int argc, char** argv)
{
	try
	{
		po::options_description desc("Allowed options");
		desc.add_options()
			("help,h", "Show help message")
			("optimize,o",po::bool_switch()->default_value(false), "Enable the optimizer")
			("scale", po::value<float>()->default_value(1.0), "Set the UI scaling factor")
			("sources", po::value<std::vector<std::string>>(), "Solidity sources");

		po::positional_options_description positional;
		positional.add("sources", -1);

		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).positional(positional).run(), vm);
		po::notify(vm);

		if (vm.count("help"))
		{
			std::cout << desc << "\n";
			return 0;
		}

		if (!vm.count("sources"))
		{
			std::cout << "No Solidity sources provided. Use --sources to specify them.\n";
			exit(1);
		}

		std::vector<std::string> sourceList = vm["sources"].as<std::vector<std::string>>();
		std::set<std::string> sources(sourceList.begin(), sourceList.end());

		if (vm.count("sources"))
		{
			for (auto const& file: sources)
				if (!bf::exists(bf::path(file)))
				{
					std::cout << "Error: " << file << ": No such file or directory.\n";
					exit(1);
				}
		}
		bool optimize = vm["optimize"].as<bool>();
		float scale = vm["scale"].as<float>();

		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0)
		{
			printf("Error: %s\n", SDL_GetError());
			return -1;
		}

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

		SDL_Window* window = SDL_CreateWindow(
			"ethdebug tool",
			SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED,
			1280,
			720,
			SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
		if (!window)
		{
			printf("Error creating SDL Window: %s\n", SDL_GetError());
			SDL_Quit();
			return -1;
		}

		SDL_GLContext gl_context = SDL_GL_CreateContext(window);
		if (!gl_context)
		{
			printf("Failed to create OpenGL context!\n");
			SDL_DestroyWindow(window);
			SDL_Quit();
			return -1;
		}

		SDL_GL_SetSwapInterval(1);

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();

		ImGui::StyleColorsDark();

		ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
		ImGui_ImplOpenGL3_Init("#version 330");

		ethdebug::UserInterface ui(std::make_shared<ethdebug::Compiler>(sources, optimize), scale);

		bool done = false;
		while (!done)
		{
			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				ImGui_ImplSDL2_ProcessEvent(&event);

				if (event.type == SDL_QUIT)
					done = true;
			}

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplSDL2_NewFrame();
			ImGui::NewFrame();

			ui.render();

			ImGui::Render();
			glViewport(0, 0, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));
			glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
			glClear(GL_COLOR_BUFFER_BIT);

			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			SDL_GL_SwapWindow(window);
		}

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();

		SDL_GL_DeleteContext(gl_context);
		SDL_DestroyWindow(window);
		SDL_Quit();
	}
	catch (po::error const& e)
	{
		std::cerr << "Error: " << e.what() << "\n";
		return 1;
	}
	catch (std::exception const& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
		return 1;
	}

	return 0;
}
