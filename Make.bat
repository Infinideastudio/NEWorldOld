@echo on
@echo ��ʼ����..............
md NEWorld_Release
md NEWorld_Release\Audio
md NEWorld_Release\Fonts
md NEWorld_Release\Lang
md NEWorld_Release\Shaders
md NEWorld_Release\Textures
md NEWorld_Release\�޷�������Ϸ�����ļ��а�װ���п�
copy Audio\*.* NEWorld_Release\Audio 
copy Fonts\*.* NEWorld_Release\Fonts 
copy Lang\*.* NEWorld_Release\Lang 
copy Shaders\*.* NEWorld_Release\Shaders 
copy Textures\*.* NEWorld_Release\Textures 
copy �޷�������Ϸ�����ļ��а�װ���п� NEWorld_Release\�޷�������Ϸ�����ļ��а�װ���п� 
copy glfw3.dll NEWorld_Release\glfw3.dll
copy NEWorld.exe NEWorld_Release\NEWorld.exe
@pause