#include "stdafx.h"
#include "window.h"

beluga::MainWindow mainWindow;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
  _In_opt_ HINSTANCE hPrevInstance,
  _In_ LPWSTR  lpCmdLine,
  _In_ int    nCmdShow) {

  if (mainWindow.Create(hInstance, WndProc) == NULL) {
    return 0;
  }

  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  return mainWindow.OnMessage(hwnd, msg, wp, lp);
}
