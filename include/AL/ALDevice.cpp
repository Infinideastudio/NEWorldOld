#include "ALDevice.h"
ALDeviceList * ALDevice::GetALDeviceList()//������Delete
{
	ALDeviceList * pDeviceList = new ALDeviceList();
	return pDeviceList;
}
bool ALDevice::InitAL(ALCchar * DeviceName)//��ʼ��
{
	Device = alcOpenDevice(DeviceName);
	if (Device)
	{
		Context = alcCreateContext(Device, NULL);
		if (Context)
		{
			alcMakeContextCurrent(Context);
			//����EFX
			EFX::Init();
			return true;
		}
		alcCloseDevice(Device);
	}
	return false;
}
void ALDevice::Updatelistener(ALfloat listenerPos[], ALfloat listenerVel[], ALfloat listenerOri[])
{
	alListenerfv(AL_POSITION, listenerPos);
	alListenerfv(AL_VELOCITY, listenerVel);
	alListenerfv(AL_ORIENTATION, listenerOri);
}
void ALDevice::Updatesource(ALuint Source, ALfloat sourcePos[], ALfloat sourceVel[])
{
	alSourcefv(Source, AL_POSITION, sourcePos);
	alSourcefv(Source, AL_VELOCITY, sourceVel);
}
bool ALDevice::load(char * WavFileName, ALuint *uiBuffer)
{
	alGenBuffers(1, uiBuffer);

	CWaves WaveLoader;
	WAVEID			WaveID;
	ALint			iDataSize, iFrequency;
	ALenum			eBufferFormat;
	ALchar			*pData;
	if (SUCCEEDED(WaveLoader.LoadWaveFile(WavFileName, &WaveID))) {
		if ((SUCCEEDED(WaveLoader.GetWaveSize(WaveID, (unsigned long*)&iDataSize))) &&
			(SUCCEEDED(WaveLoader.GetWaveData(WaveID, (void**)&pData))) &&
			(SUCCEEDED(WaveLoader.GetWaveFrequency(WaveID, (unsigned long*)&iFrequency))) &&
			(SUCCEEDED(WaveLoader.GetWaveALBufferFormat(WaveID, &alGetEnumValue, (unsigned long*)&eBufferFormat))))
		{
			alBufferData(*uiBuffer, eBufferFormat, pData, iDataSize, iFrequency);
			WaveLoader.DeleteWaveFile(WaveID);
			return true;
		}
	}

	
	return false;
}
ALuint ALDevice::Play(ALuint uiBuffer, bool loop, float gain,  ALfloat sourcePos[], ALfloat sourceVel[])
{
	ALuint uiSource;
	alGenSources(1, &uiSource);
	alSourcei(uiSource, AL_BUFFER, uiBuffer);
	alSourcei(uiSource, AL_LOOPING, loop);  // ������Ƶ�����Ƿ�Ϊѭ�����ţ�AL_FALSE�ǲ�ѭ�� 
	alSourcef(uiSource, AL_GAIN, gain);  //����������С��1.0f��ʾ���������openAL��̬����������С����������� 
	//Ϊʡ�£�ֱ��ͳһ����˥������
	alSourcef(uiSource, AL_ROLLOFF_FACTOR, 5.0);
	//alSourcef(uiSource, AL_MAX_DISTANCE, 30.0);
	alSourcef(uiSource, AL_REFERENCE_DISTANCE, 1.0);
	//����λ��
	Updatesource(uiSource,sourcePos,sourceVel);
	//����EFX
	EFX::set(uiSource);
	alSourcePlay(uiSource);
	return uiSource;
}
void ALDevice::Stop(ALuint Source)
{
	alSourceStop(Source);
	alDeleteSources(1, &Source);
}
void ALDevice::unload(ALuint uiBuffer)
{
	alDeleteBuffers(1, &uiBuffer);
}

