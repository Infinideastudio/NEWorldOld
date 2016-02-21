@echo on
@echo 开始生成..............
md NEWorld_Release
md NEWorld_Release\Audio
md NEWorld_Release\Fonts
md NEWorld_Release\Lang
md NEWorld_Release\Shaders
md NEWorld_Release\Textures
md NEWorld_Release\无法运行游戏请点击文件夹安装运行库
copy Audio\*.* NEWorld_Release\Audio 
copy Fonts\*.* NEWorld_Release\Fonts 
copy Lang\*.* NEWorld_Release\Lang 
copy Shaders\*.* NEWorld_Release\Shaders 
copy Textures\*.* NEWorld_Release\Textures 
copy 无法运行游戏请点击文件夹安装运行库 NEWorld_Release\无法运行游戏请点击文件夹安装运行库 
copy glfw3.dll NEWorld_Release\glfw3.dll
copy NEWorld.exe NEWorld_Release\NEWorld.exe
@pause