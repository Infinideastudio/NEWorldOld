#include"AudioSystem.h"
namespace AudioSystem {
	ALDevice Device;
	//Gain
	ALfloat BGMGain =0.1f;//��������
	ALfloat SoundGain = 0.17f;//��Ч
	//Set
	ALenum DopplerModel = AL_INVERSE_DISTANCE_CLAMPED;//����OpenAL�ľ���ģ��
	ALfloat DopplerFactor = 1.0f;//����������
	ALfloat SpeedOfSound = Air_SpeedOfSound;//����
	//Update
	bool FallBefore = false;//OnGround
	bool DownWaterBefore = false;//InWater
	int BGMNum = 0;
	//Buffer
	ALuint BGM[10];
	ALuint Run = -1;
	ALuint Click = -1;
	ALuint Fall = -1;
	ALuint BlockClick = -1;
	ALuint DownWater = -1;
	//Source
	ALuint SBGM = -1;
	ALuint SRun = -1;
	ALuint SClick = -1;
	ALuint SFall = -1;
	ALuint SBlockClick = -1;
	ALuint SDownWater = -1;
	void Init() {
		//��ʼ���豸
		ALDeviceList *DL = Device.GetALDeviceList();
		Device.InitAL(DL->GetDeviceName(DL->GetDefaultDevice()));
		delete DL;
		//�������й���
		alEnable(AL_DOPPLER_FACTOR);
		alEnable(AL_DISTANCE_MODEL);
		alEnable(AL_SPEED_OF_SOUND);
		//��������
		char BGMName[256];
		for (size_t i = 0; i < 10; i++)
		{
			BGM[i] = -1;
		}
		for (size_t i = 0; i < 10; i++)
		{
			sprintf_s(BGMName, "Audio\\BGM%d.wav", i);
			if (Device.load(BGMName, &BGM[BGMNum])) {
				BGMNum++;
			}
		}
		//����and�ܲ�����
		if (!Device.load("Audio\\Run.wav", &Run))Run = -1;
		//��굥��
		if (!Device.load("Audio\\Click.wav", &Click))Click = -1;
	    //����
		if (!Device.load("Audio\\Fall.wav", &Fall))Fall = -1;
	    //���򷽿�
		if (!Device.load("Audio\\BlockClick.wav", &BlockClick))BlockClick = -1;
		//��ˮ
		if (!Device.load("Audio\\DownWater.wav", &DownWater))DownWater = -1;
	    //����BGM
		int size = GetTickCount64() % BGMNum;
		ALfloat Pos[] = { 0.0,0.0,0.0 };
		ALfloat Vel[] = { 0.0,0.0,0.0 };
		SBGM = Device.Play(BGM[size], false, BGMGain, Pos, Vel);
	}
	void Update(ALfloat PlayerPos[3],bool BFall, bool BBlockClick, ALfloat BlockPos[3], int BRun,bool BDownWater) {
		//����ȫ�ֳ���
		alDopplerFactor(DopplerFactor);
		alDistanceModel(DopplerModel);
		alSpeedOfSound(SpeedOfSound);
		//��������
		if (SBGM != -1)alSourcef(SBGM,AL_GAIN,BGMGain);
		if (SRun != -1)alSourcef(SRun, AL_GAIN, SoundGain);
		if (SClick != -1)alSourcef(SClick, AL_GAIN, SoundGain);
		if (SFall != -1)alSourcef(SFall, AL_GAIN, SoundGain);
		if (SBlockClick != -1)alSourcef(SBlockClick, AL_GAIN, SoundGain);
		if (SDownWater != -1)alSourcef(SDownWater, AL_GAIN, SoundGain);
		//���»���
		if (SBGM != -1)EFX::set(SBGM);
		if (SRun != -1)EFX::set(SRun);
		if (SClick != -1)EFX::set(SClick);
		if (SFall != -1)EFX::set(SFall);
		if (SBlockClick != -1)EFX::set(SBlockClick);
		if (SDownWater != -1)EFX::set(SDownWater);
		//�������λ��
		PlayerPos[1] += 0.74;
		ALfloat Vel[] = { 0.0,0.0,0.0 };
		ALfloat Ori[] = { 0.0,0.0,-1.0, 0.0,1.0,0.0 };
		Device.Updatelistener(PlayerPos, Vel, Ori);
		//����BGMλ��
		ALint state;
		alGetSourcei(SBGM, AL_SOURCE_STATE, &state);
		if (state == AL_STOPPED)
		{
			Device.Stop(SBGM);
			int size = GetTickCount64() % BGMNum;
			ALfloat Pos[] = { 0.0,0.0,0.0 };
			ALfloat Vel[] = { 0.0,0.0,0.0 };
			SBGM = Device.Play(BGM[size], false, BGMGain, Pos, Vel);
		}
		Device.Updatesource(SBGM, PlayerPos, Vel);
		//����
		PlayerPos[1] -= 1.54;
		if (BFall != FallBefore)
		{
			if (BFall) {
				SFall = Device.Play(Fall, false, SoundGain, PlayerPos, Vel);
			}
			FallBefore = BFall;
		}
		else
		{
			if(SFall!=-1)Device.Stop(SFall);
			SFall = -1;
		}
		//���򷽿�
		if (BBlockClick)
		{
			if (SBlockClick==-1) {
				SBlockClick = Device.Play(BlockClick, true, SoundGain, BlockPos, Vel);
			}
		}
		else
		{
			if(SBlockClick!=-1)Device.Stop(SBlockClick);
			SBlockClick = -1;
		}
		//����
		if ((BRun!=0)&&BFall)
		{
			if (SRun == -1)
			{
				SRun = Device.Play(Run, true, SoundGain, PlayerPos, Vel);
			}
			Device.Updatesource(SRun, PlayerPos, Vel);
			alSourcef(SRun, AL_PITCH, BRun*0.5f);
		}
		else
		{
			if(SRun!=-1)Device.Stop(SRun);
			SRun = -1;
		}
		//��ˮ
		if (BDownWater != DownWaterBefore)
		{
			if (SDownWater == -1)SDownWater = Device.Play(DownWater, false, SoundGain, PlayerPos, Vel);
			DownWaterBefore = BDownWater;
		}
		else
		{
			if (SDownWater != -1) {
				ALint state;
				alGetSourcei(SDownWater, AL_SOURCE_STATE, &state);
				if (state == AL_STOPPED)
				{
					Device.Stop(SDownWater);
					SDownWater = -1;
				}
			}
		}
	}
	void ClickEvent() {
		ALfloat Pos[] = { 0.0,0.0,0.0 };
		ALfloat Vel[] = { 0.0,0.0,0.0 };
		SClick = Device.Play(Click, false, SoundGain, Pos, Vel);
		Sleep(50);
		Device.Stop(SClick);
		SClick = -1;
	}
	void UnInit() {
		if (SBGM != -1)Device.Stop(SBGM);
		if (SRun != -1)Device.Stop(SRun);
		if (SClick != -1)Device.Stop(SClick);
		if (SFall != -1)Device.Stop(SFall);
		if (SBlockClick != -1)Device.Stop(SBlockClick);
		if (SDownWater != -1)Device.Stop(SDownWater);

		for (size_t i = 0; i < 10; i++)
		{
			if (BGM[i] != -1)Device.unload(BGM[i]);
		}
		if (Run != -1)Device.unload(Run);
		if (Click != -1)Device.unload(Click);
		if (Fall != -1)Device.unload(Fall);
		if (BlockClick != -1)Device.unload(BlockClick);
		if (DownWater != -1)Device.unload(DownWater);

		Device.ShutdownAL();
	}
}