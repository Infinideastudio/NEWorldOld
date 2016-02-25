#pragma once
#include<AL\ALDevice.h>
namespace AudioSystem {
	extern ALDevice Device;
	//Gain
	extern ALfloat BGMGain;//��������
	extern ALfloat SoundGain;//��Ч
	//Set
	extern ALenum DopplerModel;//����OpenAL�ľ���ģ��
	extern ALfloat DopplerFactor;//����������
	extern ALfloat SpeedOfSound;//����
	const ALfloat Air_SpeedOfSound = 343.3f;
	const ALfloat Water_SpeedOfSound = 1473.0f;
	//Update
	extern bool FallBefore;//OnGround
	extern bool DownWaterBefore;//InWater
	extern int BGMNum;
	//Buffer
	extern ALuint BGM[10];
	extern ALuint Run;
	extern ALuint Click;
	extern ALuint Fall;
	extern ALuint BlockClick;
	extern ALuint DownWater;
	//Source
	extern ALuint SBGM;
	extern ALuint SRun;
	extern ALuint SClick;
	extern ALuint SFall;
	extern ALuint SBlockClick;
	extern ALuint SDownWater;
	void Init();
	void Update(ALfloat PlayerPos[3],bool BFall, bool BBlockClick, ALfloat BlockPos[3], int Run, bool BDownWater);
	void ClickEvent();
	void UnInit();
}