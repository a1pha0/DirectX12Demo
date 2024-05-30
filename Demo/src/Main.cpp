#include "DX12Lib/Game.h"

int CALLBACK wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
	try
	{
		DX12Lib::Game game(hInstance);
		if (game.Init())
		{
			return game.Run();
		}
	}
	catch (DX12Lib::DxException& e)
	{
		MessageBoxW(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
	}

	return 0;
}
