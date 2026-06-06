#include "App.h"
#include <Board.h>
#include <Engine.h>

int main()
{
	std::cout << "[SYSTEM] Initializing UI...\n";
	App window;

	if (!window.Init())
	{
		std::cerr << "[FATAL ERROR] Window/DirectX initialization failed.\n";
		return 0;
	}
	std::cout << "[SUCCESS] UI initialized, setting up board.\n";
	Board board;
	std::cout << "[SUCCESS] Board initialized.\n";

	
	while (!window.isDone())
	{
		window.Update(board);
	}
	window.Shutdown();
	std::cout << "[FINISH] Process exited safely. Goodbye.\n";
}