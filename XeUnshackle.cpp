//==========================================================================================================================
//
//											- XeUnshackle BETA -
//			A simple app designed to apply a full set of freeboot/xebuild kernel & HV patches to a running system
//			after running the Xbox360BadUpdate HV exploit. Sets up & loads a version of launch.xex (Dashlaunch)
//          designed to run from hdd or usb root rather than flash (nand).
//
// Created by: Byrom
// 
// Credits: 
//          grimdoomer - Xbox360BadUpdate exploit.
//          cOz - Dashlaunch, xeBuild patches and much more.
//          Visual Studio / GoobyCorp
//          Diamond
//          InvoxiPlayGames - FreeMyXe, Usbdsec patches, RoL restore and general help.
//          Jeff Hamm - https://www.youtube.com/watch?v=PantVXVEVUg - Chain break video
//          ikari - freeBOOT
//          Xbox360Hub Discord #coding-corner
//          Anyone else who has contributed anything to the 360 scene. Apologies if any credits were missed.
// 
// Notes: 
//          This is basically what I came up with during initial testing so could prob be simplified & improved a lot.

// Extras: 
// 
//
//==========================================================================================================================


#include <string>
#include <fstream>
#include <xmedia2.h>
#include "stdafx.h"
#include <xtl.h> 
#include <xaudio2.h>
#include <stdio.h> 
FLOAT APP_VERS = 1.03;

static bool initialized = false;
static bool m_killed = false;
const CHAR* g_strMovieName = "embed:\\VID";
const CHAR* g_strAudioName = "embed:\\AUD";

// Get global access to the main D3D device
extern D3DDevice* g_pd3dDevice;
DWORD YellowText = 0xFFFFFF00;
DWORD WhiteText = 0xFFFFFFFF;
DWORD GreenText = 0xFF00FF00;
DWORD BlueText = 0xFF0000FF; 
DWORD PurpleText = 0xFFFF00FF; 

BOOL bShouldPlaySuccessVid = FALSE;
WCHAR wTitleHeaderBuf[100];
WCHAR wCPUKeyBuf[150];
WCHAR wDVDKeyBuf[50];
WCHAR wConTypeBuf[50];
// Globals
IXAudio2*             g_pXAudio2;
IXAudio2MasteringVoice* g_pMasterVoice;
IXAudio2SourceVoice*  g_pSourceVoice;
static BYTE* gAlignedAudioData = nullptr;

class VoiceCallback : public IXAudio2VoiceCallback {
public:
    void STDMETHODCALLTYPE OnBufferStart(void* pBufferContext) override {
        // Can add log here if needed
    }
    void STDMETHODCALLTYPE OnBufferEnd(void* pBufferContext) override {
        if (pBufferContext) {
            _aligned_free(pBufferContext);
        }
    }
    void STDMETHODCALLTYPE OnStreamEnd() override {}
    void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override {}
    void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
    void STDMETHODCALLTYPE OnLoopEnd(void*) override {}
    void STDMETHODCALLTYPE OnVoiceError(void*, HRESULT) override {}
};

static VoiceCallback gVoiceCallback;
void ShowNotify(PWCHAR NotifyText);


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class XeUnshackle : public ATG::Application
{

    // Pointer to XMV player object.
    IXMedia2XmvPlayer* m_xmvPlayer;
	IXMedia2XmvPlayer* m_audioPlayer;
    // Structure for controlling where the movie is played.
    XMEDIA_VIDEO_SCREEN m_videoScreen;
	
    
    // Tell XMV player about scaling and rotation parameters.
    VOID            InitVideoScreen();

    // Buffer for holding XMV data when playing from memory.
    VOID* m_movieBuffer;
	
    // XAudio2 object.
    IXAudio2* m_pXAudio2;

    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    BOOL m_bFailed;

private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
};


void SeedRandomXbox360()
{
    LARGE_INTEGER perfCount;
    QueryPerformanceCounter(&perfCount);
    srand((unsigned int)(perfCount.QuadPart & 0xFFFFFFFF));
}

//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT XeUnshackle::Initialize()
{
	SeedRandomXbox360();
    m_xmvPlayer = 0;
    m_movieBuffer = 0;

    // Initialize the XAudio2 Engine. The XAudio2 Engine is needed for movie playback.
    UINT32 flags = 0;
#ifdef _DEBUG
    flags |= XAUDIO2_DEBUG_ENGINE;
#endif

    HRESULT hr = XAudio2Create(&m_pXAudio2, flags);
    if (FAILED(hr))
        ATG::FatalError("Error %#X calling XAudio2Create\n", hr);

    IXAudio2MasteringVoice* pMasteringVoice = NULL;
    hr = m_pXAudio2->CreateMasteringVoice(&pMasteringVoice);
    if (FAILED(hr))
        ATG::FatalError("Error %#X calling CreateMasteringVoice\n", hr);

    // Create the font
    if (FAILED(m_Font.Create("embed:\\FONT")))
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow(ATG::GetTitleSafeArea());

    m_bFailed = FALSE;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: InitVideoScreen()
// Desc: Adjust how the movie is displayed on the screen. Horizontal and vertical
//      scaling and rotation are applied.
//--------------------------------------------------------------------------------------
VOID XeUnshackle::InitVideoScreen()
{
    const int width = m_d3dpp.BackBufferWidth;
    const int height = m_d3dpp.BackBufferHeight;
    const int hWidth = width / 2;
    const int hHeight = height / 2;

    // Parameters to control scaling and rotation of video.
    float m_angle = 0.0;
    float m_xScale = 1.0;
    float m_yScale = 1.0;

    // Scale the output width.
    float left = -hWidth * m_xScale;
    float right = hWidth * m_xScale;
    float top = -hHeight * m_yScale;
    float bottom = hHeight * m_yScale;

    float cosTheta = cos(m_angle);
    float sinTheta = sin(m_angle);

    // Apply the scaling and rotation.
    m_videoScreen.aVertices[0].fX = hWidth + (left * cosTheta - top * sinTheta);
    m_videoScreen.aVertices[0].fY = hHeight + (top * cosTheta + left * sinTheta);
    m_videoScreen.aVertices[0].fZ = 0;

    m_videoScreen.aVertices[1].fX = hWidth + (right * cosTheta - top * sinTheta);
    m_videoScreen.aVertices[1].fY = hHeight + (top * cosTheta + right * sinTheta);
    m_videoScreen.aVertices[1].fZ = 0;

    m_videoScreen.aVertices[2].fX = hWidth + (left * cosTheta - bottom * sinTheta);
    m_videoScreen.aVertices[2].fY = hHeight + (bottom * cosTheta + left * sinTheta);
    m_videoScreen.aVertices[2].fZ = 0;

    m_videoScreen.aVertices[3].fX = hWidth + (right * cosTheta - bottom * sinTheta);
    m_videoScreen.aVertices[3].fY = hHeight + (bottom * cosTheta + right * sinTheta);
    m_videoScreen.aVertices[3].fZ = 0;

    // Always leave the UV coordinates at the default values.
    m_videoScreen.aVertices[0].fTu = 0;
    m_videoScreen.aVertices[0].fTv = 0;
    m_videoScreen.aVertices[1].fTu = 1;
    m_videoScreen.aVertices[1].fTv = 0;
    m_videoScreen.aVertices[2].fTu = 0;
    m_videoScreen.aVertices[2].fTv = 1;
    m_videoScreen.aVertices[3].fTu = 1;
    m_videoScreen.aVertices[3].fTv = 1;

    // Tell the XMV player to use the new settings.
    // This locks the vertex buffer so it may cause stalls if called every frame.
    m_xmvPlayer->SetVideoScreen(&m_videoScreen);
}

// Log function for audio debugging, not really needed anymore if you export the wav file your trying to play in the correct format. but it does give useful info about your wav and if its being parsed correctly.
void WriteAudioLog(const char* logMessage)
{
    const char* filePath = "GAME:\\log.txt";

    FILE* file = fopen(filePath, "a"); 
    if (file)
    {
        fprintf(file, "%s\n", logMessage);
        fclose(file);
    }
    else
    {
        
        std::string narrowPath(filePath);
        std::wstring widePath(narrowPath.begin(), narrowPath.end());

        std::wstring errorMsg = L"File failed to save! Loc: " + widePath;
        ShowNotify((PWCHAR)errorMsg.c_str());
    }
}

//--------------------------------------------------------------------------------------
// Name: PlayAudioFromMemory
// Desc: Parses an 8-bit unsigned PCM WAV file from memory, sets up XAudio2 source voices,
// and submits the audio buffer for playback to play simple menu music after the intro video.
//--------------------------------------------------------------------------------------


void PlayAudioFromMemory()
{
    VOID* pAudioData = nullptr;
    DWORD dwAudioSize = 0;

    HMODULE hModule = GetModuleHandle(NULL);
    if (!XGetModuleSection(hModule, "AUD", &pAudioData, &dwAudioSize))
    {
        return;
    }
    BYTE* p = static_cast<BYTE*>(pAudioData);

    if (dwAudioSize < 44 || memcmp(p, "RIFF", 4) != 0 || memcmp(p + 8, "WAVE", 4) != 0)
    {
        WriteAudioLog("Invalid WAV header. :( \n");
        return;
    }

    WriteAudioLog("RIFF WAVE header validated. :^) \n");

    DWORD offset = 12;
    bool foundFmt = false, foundData = false;

    BYTE* pFmtChunkData = nullptr;
    DWORD fmtChunkSize = 0;
    BYTE* pDataChunkData = nullptr;
    DWORD dataChunkSize = 0;

    while (offset + 8 <= dwAudioSize)
    {
        BYTE* chunkID = p + offset;

        DWORD chunkSize = 
            (DWORD)p[offset + 4] |
            ((DWORD)p[offset + 5] << 8) |
            ((DWORD)p[offset + 6] << 16) |
            ((DWORD)p[offset + 7] << 24);

        DWORD chunkDataSize = chunkSize;
        DWORD padding = (chunkSize % 2) ? 1 : 0;

        DWORD nextOffset = offset + 8 + chunkDataSize + padding;
        if (nextOffset > dwAudioSize)
        {
            char log[128];
            sprintf_s(log, sizeof(log), "Chunk size too large at offset %u (chunkSize %u)\n", offset, chunkSize);
            WriteAudioLog(log);
            return;
        }

        if (memcmp(chunkID, "fmt ", 4) == 0)
        {
            if (chunkSize < 16)
            {
                WriteAudioLog("fmt chunk too small. :( \n");
                return;
            }
            pFmtChunkData = p + offset + 8;
            fmtChunkSize = chunkSize;
            foundFmt = true;
        }
        else if (memcmp(chunkID, "data", 4) == 0)
        {
            pDataChunkData = p + offset + 8;
            dataChunkSize = chunkSize;
            foundData = true;
        }

        offset = nextOffset;
    }

    if (!foundFmt || !foundData)
    {
        WriteAudioLog("Missing fmt or data chunk.\n");
        return;
    }

    if (!pFmtChunkData)
    {
        WriteAudioLog("fmt chunk data pointer null.\n");
        return;
    }

    WAVEFORMATEX wfx = {};
    BYTE* c = pFmtChunkData;

    wfx.wFormatTag = c[0] | (c[1] << 8);
    wfx.nChannels = c[2] | (c[3] << 8);
    wfx.nSamplesPerSec = c[4] | (c[5] << 8) | (c[6] << 16) | (c[7] << 24);
    wfx.nAvgBytesPerSec = c[8] | (c[9] << 8) | (c[10] << 16) | (c[11] << 24);
    wfx.nBlockAlign = c[12] | (c[13] << 8);
    wfx.wBitsPerSample = c[14] | (c[15] << 8);
    wfx.cbSize = 0;
    if (fmtChunkSize > 16)
    {
        wfx.cbSize = c[16] | (c[17] << 8);
    }

    char log[1024];
    sprintf_s(log, sizeof(log),
        "Parsed fmt chunk: formatTag=0x%04X, channels=%d, sampleRate=%d, avgBytesPerSec=%d, blockAlign=%d, bitsPerSample=%d, cbSize=%d\n",
        wfx.wFormatTag, wfx.nChannels, wfx.nSamplesPerSec, wfx.nAvgBytesPerSec, wfx.nBlockAlign, wfx.wBitsPerSample, wfx.cbSize);
    WriteAudioLog(log);

    if (wfx.wFormatTag != WAVE_FORMAT_PCM)
    {
        WriteAudioLog("Only PCM format supported. :/\n");
        return;
    }

    // Validate bits per sample
    if (wfx.wBitsPerSample != 8)
    {
        WriteAudioLog("Unsupported BitsPerSample. Only 8-bit unsigned PCM is supported. :/ \n");
        return;
    }

    // Initialize XAudio2.
    if (!g_pXAudio2)
    {
        if (FAILED(XAudio2Create(&g_pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR)))
        {
            WriteAudioLog("XAudio2Create failed. :( \n");
            return;
        }
        if (FAILED(g_pXAudio2->CreateMasteringVoice(&g_pMasterVoice)))
        {
            WriteAudioLog("CreateMasteringVoice failed. :( \n");
            return;
        }
    }

    if (g_pSourceVoice)
    {
        g_pSourceVoice->Stop(0);
        g_pSourceVoice->DestroyVoice();
        g_pSourceVoice = nullptr;
    }
    if (gAlignedAudioData)
    {
        _aligned_free(gAlignedAudioData);
        gAlignedAudioData = nullptr;
    }

    HRESULT hr = g_pXAudio2->CreateSourceVoice(&g_pSourceVoice, &wfx, 0, XAUDIO2_DEFAULT_FREQ_RATIO, &gVoiceCallback);
    if (FAILED(hr))
    {
        WriteAudioLog("CreateSourceVoice failed. :( \n");
        return;
    }

    gAlignedAudioData = (BYTE*)_aligned_malloc(dataChunkSize, 16);
    if (!gAlignedAudioData)
    {
        WriteAudioLog("Failed to allocate aligned buffer. :( \n");
        g_pSourceVoice->DestroyVoice();
        g_pSourceVoice = nullptr;
        return;
    }
    memcpy(gAlignedAudioData, pDataChunkData, dataChunkSize);

    XAUDIO2_BUFFER buffer = {};
    buffer.AudioBytes = dataChunkSize;
    buffer.pAudioData = gAlignedAudioData;
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    buffer.pContext = gAlignedAudioData;

    hr = g_pSourceVoice->SubmitSourceBuffer(&buffer);
    if (FAILED(hr))
    {
        WriteAudioLog("SubmitSourceBuffer failed. :( \n");
        _aligned_free(gAlignedAudioData);
        gAlignedAudioData = nullptr;
        g_pSourceVoice->DestroyVoice();
        g_pSourceVoice = nullptr;
        return;
    }

    g_pSourceVoice->SetVolume(1.0f);

    hr = g_pSourceVoice->Start(0);
    if (FAILED(hr))
    {
        WriteAudioLog("Start playback failed. :( \n");
        _aligned_free(gAlignedAudioData);
        gAlignedAudioData = nullptr;
        g_pSourceVoice->DestroyVoice();
        g_pSourceVoice = nullptr;
        return;
    }

    WriteAudioLog("Playback started successfully. :D\n");
}

//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
// The videos get randomly selected & played from here
// The menu music also gets called from here.
//--------------------------------------------------------------------------------------


HRESULT XeUnshackle::Update()
{
    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    if (m_xmvPlayer)
    {
        // 'A' means cancel the movie. 
        if (pGamepad->wPressedButtons & XINPUT_GAMEPAD_A)
        {
            m_xmvPlayer->Stop(XMEDIA_STOP_IMMEDIATE);
			
        }
    }
    else
    {
        // Play the movie if required
        if (bShouldPlaySuccessVid)
        {
            XMEDIA_XMV_CREATE_PARAMETERS XmvParams;
            ZeroMemory(&XmvParams, sizeof(XmvParams));

            XmvParams.dwAudioStreamId = XMEDIA_STREAM_ID_USE_DEFAULT;
            XmvParams.dwVideoStreamId = XMEDIA_STREAM_ID_USE_DEFAULT;

            bShouldPlaySuccessVid = FALSE;
            m_bFailed = FALSE;

            const char* vidSections[] = { 
    "VID", "VID1", "VID2", "VID3", "VID4", "VID5", "VID6", "VID7", "VID8", 
    "VID9", "VID10", "VID11", "VID12", "VID13", "VID14", "VID15", "VID16", "VID17", "VID18"
};
            const int vidCount = sizeof(vidSections) / sizeof(vidSections[0]);

            int randomIndex = rand() % vidCount;
            const char* selectedSection = vidSections[randomIndex];

            VOID* pSectionData = nullptr;
            DWORD dwSectionSize = 0;
            HMODULE hModule = GetModuleHandle(NULL);

            if (XGetModuleSection(hModule, selectedSection, &pSectionData, &dwSectionSize))
            {
                XmvParams.createType = XMEDIA_CREATE_FROM_MEMORY;
                XmvParams.createFromMemory.pvBuffer = pSectionData;
                XmvParams.createFromMemory.dwBufferSize = dwSectionSize;

                HRESULT hr = XMedia2CreateXmvPlayer(m_pd3dDevice, m_pXAudio2, &XmvParams, &m_xmvPlayer);
                if (SUCCEEDED(hr))
                {
                    InitVideoScreen();
                }
                else
                {
                    m_bFailed = TRUE;
                }
            }
            else
            {
                m_bFailed = TRUE;
            }
        }
    }

    if (!DisableButtons)
    {
        if (pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK)
        {
            XLaunchNewImage(XLAUNCH_KEYWORD_DEFAULT_APP, 0);
        }
        else if (pGamepad->wPressedButtons & XINPUT_GAMEPAD_X)
        {
            SaveConsoleDataToFile();
        }
        else if (pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y)
        {
            Dump1blRomToFile();
        }
		else if (pGamepad->wPressedButtons & XINPUT_GAMEPAD_B)
		{
		  if(!m_killed)
		  {
		  ShowNotify(L"Menu music killed!");
		  g_pSourceVoice->DestroyVoice(); //
		  m_killed = true;
		  }
		}
    }
	
    return S_OK;
}



//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT XeUnshackle::Render()
{
    // Draw a gradient filled background
    //ATG::RenderBackground(0xff0000ff, 0xff000000);

    // If we are currently playing a movie.
    if (m_xmvPlayer)
    {
		
        // If RenderNextFrame does not return S_OK then the frame was not
        // rendered (perhaps because it was cancelled) so a regular frame
        // buffer should be rendered before calling present.
        HRESULT hr = m_xmvPlayer->RenderNextFrame(0, NULL);

        // Reset our cached view of what pixel and vertex shaders are set, because
        // it is no longer accurate, since XMV will have set their own shaders.
        // This avoids problems when the shader cache thinks it knows what shader
        // is set and it is wrong.
        m_pd3dDevice->SetVertexShader(0);
        m_pd3dDevice->SetPixelShader(0);
        m_pd3dDevice->SetVertexDeclaration(0);

        if (FAILED(hr) || hr == (HRESULT)XMEDIA_W_EOF)
        {
            // Release the movie object
			
            m_xmvPlayer->Release();
            m_xmvPlayer = 0;
            // Movie playback changes various D3D states, so you should reset the
            // states that you need after movie playback is finished.
            m_pd3dDevice->SetRenderState(D3DRS_VIEWPORTENABLE, TRUE);

            // Free up any memory allocated for playing from memory.
            if (m_movieBuffer)
            {
                free(m_movieBuffer);
                m_movieBuffer = 0;
				
            }
        }

    }

    else
    {
		
        ATG::RenderBackground(0xFF00FF00, 0xFF000032); //Green blue gradient. yes i know it looks ugly, but you can change it to whatever you want. 
        m_Font.Begin();
        m_Font.SetScaleFactors(1.5f, 1.5f);
        m_Font.DrawText(0, 0, YellowText, wTitleHeaderBuf);

        // Pre-Release Build Identifier
        //m_Font.DrawText(840, 0, WhiteText, L"[TEST BUILD]");
        //

        m_Font.SetScaleFactors(1.0f, 1.0f);
		
			 
        // General info
		
        m_Font.DrawText(0, 70, YellowText, currentLocalisation->MainInfo);
		m_Font.DrawText(0, 100, PurpleText, L"Welcome to the Meme Edition build of XeUnshackle, a version where its just filled with retardation. ");
		m_Font.DrawText(0, 135, PurpleText, currentLocalisation->BKillMusic);
        // Dashlaunch Info
        m_Font.DrawText(0, 290, YellowText, wDLStatusBuf);
        if (bDLisLoaded)
        {
            m_Font.DrawText(0, 320, YellowText, currentLocalisation->MainScrDL);
        }

        // Console Info
        m_Font.DrawText(0, 460, YellowText, wConTypeBuf);
        m_Font.DrawText(0, 490, YellowText, wCPUKeyBuf);
        m_Font.DrawText(0, 520, YellowText, wDVDKeyBuf);

        m_Font.DrawText(0, 570, YellowText, L"https://github.com/Byrom90/XeUnshackle");
        m_Font.DrawText(0, 600, YellowText, L"https://byrom.uk");
		m_Font.DrawText(0, 540, PurpleText, L"https://github.com/UnstoppableWolf/");
		

        // User input with buttons - Make these white so they display correctly and stand out to the user
        m_Font.DrawText(840, 530, WhiteText, currentLocalisation->MainScrBtnSaveInfo);// X button icon with text 
        m_Font.DrawText(840, 560, WhiteText, currentLocalisation->MainScrBtnDump1BL);// Y button icon with text

        m_Font.DrawText(840, 600, WhiteText, currentLocalisation->MainScrBtnExit);// Back button icon with text
        m_Font.End();
    }

   if (!initialized && !m_xmvPlayer)
	{
		PlayAudioFromMemory();
		ShowNotify(L"XeUnshackle Meme Edition v1.03 Loaded! Patches have been applied!");
	    initialized = true;
	}
    // Present the scene
    m_pd3dDevice->Present(NULL, NULL, NULL, NULL);
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    SetLocale(); // Set the correct locale so text will be displayed in the correct language

    // Part 1 - We apply the HV patches here (if required)
    if (!Hvx::CheckPPExpHVAccess()) // If we have pp access then assume we have done this previously
    {
        if (!Hvx::DisableExpChecks()) // Stage 1 - Apply HV patches to disable checks on expansions. If this fails do not proceed
        {
            cprintf("[XeUnshackle] Stage 1 failed!");
            ShowErrorAndExit(1);
        }
        cprintf("[XeUnshackle] Stage 1 success!");
        if (!Hvx::SetupPPExpHVAccess()) // Stage 2 - Install the peek poke expansion. If this fails do not proceed
        {
            cprintf("[XeUnshackle] Stage 2 failed!");
            ShowErrorAndExit(2);
        }
        cprintf("[XeUnshackle] Stage 2 success!");
        if (!Hvx::CheckPPExpHVAccess()) // Stage 3 - Check if we now have HV access via Peek Poke expansion. If this fails do not proceed
        {
            cprintf("[XeUnshackle] Stage 3 failed!");
            ShowErrorAndExit(3);
        }
        cprintf("[XeUnshackle] Stage 3 success!");
        if (!ApplyFreebootHVPatches())
        {
            cprintf("[XeUnshackle] Stage 4 failed!");
            ShowErrorAndExit(4);
        }
        cprintf("[XeUnshackle] Stage 4 success!");

        //cprintf("[XeUnshackle] Relaunching before proceeding with Stage 5!"); // NO LONGER REQUIRED
        //RelaunchApp();

        // No more relaunching
        cprintf("[XeUnshackle] Calling KeFlushEntireTb");
        KeFlushEntireTb(); // This is called in XexpTitleTerminateNotification. Maybe this is why relaunching works???

    }
    // Part 2 - We end up here if part 1 succeeded in gaining HV access via expansions
    cprintf("[XeUnshackle] Checking kernel patch state");
    if (*(DWORD*)0x80108E70 != 0x48003134) // This is the last freeboot kernel patch applied. This determines whether we have applied them yet
    {
        if (!ApplyFreebootKernPatches())
        {
            cprintf("[XeUnshackle] Stage 5 failed!");
            ShowErrorAndExit(5);
        }
        ApplyAdditionalPatches(); // Other patches for general fixes

        RestoreRoL(); // Restore the default RoL state

        cprintf("[XeUnshackle] Calling KeFlushEntireTb");
        KeFlushEntireTb();

        cprintf("[XeUnshackle] Stage 5 success!");
    }
    // Part 3 - We should only ever begin here for any subsequent launches of the app

    // If Dashlaunch loaded successfully we can revert the patches done by BadUpdate. 
    // Needs to be like this due to Dashlaunch fixing Retail signed xex files that have been patched.
    // BadUpdate patches also allow this but prevent the Freeboot patches from functioning correctly
    // IMPORTANT NOTE: Dashlaunch doesn't appear to load the plugins until you exit to dash aka the next executable load.
    // 0 = FAILED
    // 1 = SUCCESS
    // 2 = Already loaded
    
    if (SysLoadDashlaunch() == 1) // We always call this here since it also sets up the wchar buffer to display in the app for Dashlaunch load status
    {

        RevertBadExploitPatches(); // Restore changes made by the exploit
    }

    cprintf("[XeUnshackle] All patches have been applied! Proceeding to init the ui...");
	
    // Grab some stuff for display in the ui
    ZeroMemory(wTitleHeaderBuf, sizeof(wTitleHeaderBuf));
    swprintf_s(wTitleHeaderBuf, L"%ls XeUnshackle: Meme Edition v%.2f BETA %ls", GLYPH_RIGHT_TICK, APP_VERS, GLYPH_LEFT_TICK);
    // Motherboard type
    ZeroMemory(wConTypeBuf, sizeof(wConTypeBuf));
    swprintf_s(wConTypeBuf, L"Console type: %S", GetMoboByHWFlags().c_str());
    // cpu key
    QWORD fuse3 = Hvx::HvGetFuseline(3);
    QWORD fuse5 = Hvx::HvGetFuseline(5);
    ZeroMemory(wCPUKeyBuf, sizeof(wCPUKeyBuf));
    swprintf_s(wCPUKeyBuf, L"CPUKey: %08X%08X%08X%08X", fuse3 >> 32, fuse3 & 0xffffffff, fuse5 >> 32, fuse5 & 0xffffffff);
    // dvd key
    BYTE DVDKeyBytes[16];
    QWORD kvAddress = Hvx::HvPeekQWORD(0x00000002000163C0);
    Hvx::HvPeekBytes(kvAddress + 0x100, DVDKeyBytes, 16);
    ZeroMemory(wDVDKeyBuf, sizeof(wDVDKeyBuf));
    swprintf_s(wDVDKeyBuf, L"DVDKey: %08X%08X%08X%08X", *(DWORD*)(DVDKeyBytes), *(DWORD*)(DVDKeyBytes + 4), *(DWORD*)(DVDKeyBytes + 8), *(DWORD*)(DVDKeyBytes + 12));

    BackupOrigMAC(); // This will cause a notify to pop before the video has played completely but only if it hasn't been dumped previously

    // Run the ui portion of the app with video etc...
    XeUnshackle atgApp;

    // For movie playback we want to synchronize to the monitor.
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    ATG::GetVideoSettings(&atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight);
    bShouldPlaySuccessVid = TRUE;
    atgApp.Run();
}
