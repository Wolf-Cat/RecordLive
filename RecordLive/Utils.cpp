#include "Utils.h"
#include <dshow.h>
#include <Windows.h>
#include <objbase.h>

QString Utils::GetMicrophoneDeviceName()
{
    char sName[256] = { 0 };
    QString capture = "";
    bool bRet = false;
    ::CoInitialize(NULL);

    ICreateDevEnum* pCreateDevEnum;//enumrate all audio capture devices
    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_ICreateDevEnum,
        (void**)&pCreateDevEnum);

    IEnumMoniker* pEm;
    hr = pCreateDevEnum->CreateClassEnumerator(CLSID_AudioInputDeviceCategory, &pEm, 0);
    if (hr != NOERROR)
    {
        ::CoUninitialize();
        return "";
    }

    pEm->Reset();
    ULONG cFetched;
    IMoniker *pM;
    while (hr = pEm->Next(1, &pM, &cFetched), hr == S_OK)
    {

        IPropertyBag* pBag = NULL;
        hr = pM->BindToStorage(0, 0, IID_IPropertyBag, (void**)&pBag);
        if (SUCCEEDED(hr))
        {
            VARIANT var;
            var.vt = VT_BSTR;
            hr = pBag->Read(L"FriendlyName", &var, NULL);//还有其他属性，像描述信息等等
            if (hr == NOERROR)
            {
                //获取设备名称
                WideCharToMultiByte(CP_ACP, 0, var.bstrVal, -1, sName, 256, "", NULL);
                capture = QString::fromLocal8Bit(sName);
                SysFreeString(var.bstrVal);
            }
            pBag->Release();
        }
        pM->Release();
        bRet = true;
    }
    pCreateDevEnum = NULL;
    pEm = NULL;
    ::CoUninitialize();
    return capture;
}
