https://github.com/giladreich/cmake-imgui

cmake .. -DIMGUI_STATIC_LIBRARY=OFF -DIMGUI_WITH_BACKEND=ON -DIMGUI_BACKEND_PLATFORM=WIN32 -DIMGUI_BACKEND_DX11=ON

cmake --build . --config debug --target install

or

cmake --build . --config release --target install
