#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <devpkey.h>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <algorithm>
#include <cwctype>
#include "PluginInterface.h"
#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "Hid.lib")

namespace {
struct State { std::wstring value=L"—"; std::wstring detail; int pct=-1; };
std::mutex lockState;
State mouseState, headphoneState, keyboardState;
std::atomic<bool> running{false};
std::thread worker;
const DEVPROPKEY BatteryLifeKey={{0x49CD1F76,0x5626,0x4B17,{0xA4,0xE8,0x18,0xB4,0xAA,0x1A,0x22,0x13}},10};
const DEVPROPKEY HfpBatteryKey={{0x104EA319,0x6EE2,0x4701,{0xBD,0x47,0x8D,0xDB,0xF4,0x25,0xBB,0xE5}},2};
const DEVPROPKEY FriendlyNameKey={{0xA45C254E,0xDF1C,0x4EFD,{0x80,0x20,0x67,0xD1,0x46,0xA8,0x50,0xE0}},14};
const DEVPROPKEY DeviceDescKey={{0xA45C254E,0xDF1C,0x4EFD,{0x80,0x20,0x67,0xD1,0x46,0xA8,0x50,0xE0}},2};
std::wstring lower(std::wstring s){std::transform(s.begin(),s.end(),s.begin(),[](wchar_t c){return (wchar_t)towlower(c);});return s;}
bool has(const std::wstring&s,const wchar_t*a,const wchar_t*b){auto l=lower(s);return l.find(a)!=s.npos||l.find(b)!=s.npos;}
bool readName(HDEVINFO h,SP_DEVINFO_DATA&d,const DEVPROPKEY&k,std::wstring&v){
 wchar_t b[512]{};DEVPROPTYPE t=0;DWORD n=0;
 if(!SetupDiGetDevicePropertyW(h,&d,&k,&t,(PBYTE)b,sizeof(b),&n,0)||t!=DEVPROP_TYPE_STRING)return false;
 v=b;return !v.empty();
}
bool readPct(HDEVINFO h,SP_DEVINFO_DATA&d,const DEVPROPKEY&k,int&p){
 BYTE b[16]{};DEVPROPTYPE t=0;DWORD n=0;
 if(!SetupDiGetDevicePropertyW(h,&d,&k,&t,b,sizeof(b),&n,0))return false;
 unsigned v=999;
 if(t==DEVPROP_TYPE_BYTE&&n>=1)v=b[0];
 else if(t==DEVPROP_TYPE_UINT16&&n>=2)v=*reinterpret_cast<USHORT*>(b);
 else if(t==DEVPROP_TYPE_UINT32&&n>=4)v=*reinterpret_cast<ULONG*>(b);
 else if(t==DEVPROP_TYPE_INT32&&n>=4){LONG x=*reinterpret_cast<LONG*>(b);if(x<0)return false;v=(unsigned)x;}
 if(v>100)return false;p=(int)v;return true;
}
void set(State&s,int p,const std::wstring&name){s.pct=p;s.value=std::to_wstring(p)+L"%";s.detail=name+L": "+s.value;}
void scanWindowsBatteryProperties(){
 State hp,kb; HDEVINFO h=SetupDiGetClassDevsW(nullptr,nullptr,nullptr,DIGCF_PRESENT|DIGCF_ALLCLASSES);
 if(h==INVALID_HANDLE_VALUE)return;
 for(DWORD i=0;;++i){SP_DEVINFO_DATA d{};d.cbSize=sizeof(d);if(!SetupDiEnumDeviceInfo(h,i,&d))break;
  std::wstring name;if(!readName(h,d,FriendlyNameKey,name))readName(h,d,DeviceDescKey,name);if(name.empty())continue;
  int p=-1;if(!readPct(h,d,BatteryLifeKey,p)&&!readPct(h,d,HfpBatteryKey,p))continue;
  if(hp.pct<0&&has(name,L"head",L"耳机"))set(hp,p,name);
  if(kb.pct<0&&has(name,L"keyboard",L"键盘"))set(kb,p,name);
 }
 SetupDiDestroyDeviceInfoList(h);
 std::lock_guard<std::mutex>g(lockState);
 headphoneState=hp;keyboardState=kb;
}
bool inphicVidPid(USHORT v,USHORT p){
 if(v==0x248A&&(p==0x5B2E||p==0x5B2F))return true;
 if(v==0x1D57&&p>=0xFA60&&p<=0xFA68)return true;
 return v==0x045E&&(p==0x00DB||p==0x00DC);
}
void scanInphic(){
 GUID guid{};HidD_GetHidGuid(&guid);
 HDEVINFO h=SetupDiGetClassDevsW(&guid,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
 if(h==INVALID_HANDLE_VALUE)return;
 int found=-1;std::wstring productName=L"INPHIC mouse";
 for(DWORD i=0;;++i){
  SP_DEVICE_INTERFACE_DATA id{};id.cbSize=sizeof(id);if(!SetupDiEnumDeviceInterfaces(h,nullptr,&guid,i,&id))break;
  DWORD n=0;SetupDiGetDeviceInterfaceDetailW(h,&id,nullptr,0,&n,nullptr);if(!n)continue;
  std::vector<BYTE>b(n);auto det=(SP_DEVICE_INTERFACE_DETAIL_DATA_W*)b.data();det->cbSize=sizeof(*det);
  if(!SetupDiGetDeviceInterfaceDetailW(h,&id,det,n,nullptr,nullptr))continue;
  HANDLE dev=CreateFileW(det->DevicePath,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(dev==INVALID_HANDLE_VALUE)continue;
  HIDD_ATTRIBUTES a{};a.Size=sizeof(a);
  if(HidD_GetAttributes(dev,&a)&&inphicVidPid(a.VendorID,a.ProductID)){
   wchar_t name[256]{};HidD_GetProductString(dev,name,sizeof(name));if(name[0])productName=name;
   BYTE report[64]{};DWORD got=0;
   // Accept only the documented IN10 status battery frame, not generic mouse/keyboard reports.
   if(ReadFile(dev,report,sizeof(report),&got,nullptr)&&got>=5&&report[0]==3&&report[2]==0x40&&report[4]<=100)found=report[4];
   CloseHandle(dev);if(found>=0)break;continue;
  }
  CloseHandle(dev);
 }
 SetupDiDestroyDeviceInfoList(h);
 std::lock_guard<std::mutex>g(lockState);
 if(found>=0)set(mouseState,found,productName);
}
void runWorker(){while(running.load()){scanWindowsBatteryProperties();scanInphic();for(int i=0;i<20&&running.load();++i)Sleep(100);}}
class Item:public IPluginItem{
 const wchar_t*name_;const wchar_t*id_;const wchar_t*label_;int index_;mutable std::wstring value_;
public:Item(const wchar_t*n,const wchar_t*i,const wchar_t*l,int x):name_(n),id_(i),label_(l),index_(x){}
 const wchar_t*GetItemName()const override{return name_;}
 const wchar_t*GetItemId()const override{return id_;}
 const wchar_t*GetItemLableText()const override{return label_;}
 const wchar_t*GetItemValueText()const override{std::lock_guard<std::mutex>g(lockState);value_=(index_==0?mouseState:index_==1?headphoneState:keyboardState).value;return value_.c_str();}
 const wchar_t*GetItemValueSampleText()const override{return L"100%";}
};
class Plugin:public ITMPlugin{
 Item mouse{L"INPHIC Mouse Battery",L"peripheral.inphic.mouse",L"Mouse",0};
 Item headphones{L"Bluetooth Headphones Battery",L"peripheral.bluetooth.headphones",L"Headphones",1};
 Item keyboard{L"Wireless Keyboard Battery",L"peripheral.wireless.keyboard",L"Keyboard",2};
public:
 IPluginItem*GetItem(int i)override{switch(i){case 0:return &mouse;case 1:return &headphones;case 2:return &keyboard;default:return nullptr;}}
 void DataRequired()override{if(!running.exchange(true))worker=std::thread(runWorker);}
 const wchar_t*GetInfo(PluginInfoIndex i)override{switch(i){case TMI_NAME:return L"Peripheral Battery";case TMI_DESCRIPTION:return L"INPHIC mouse, Bluetooth headphones and wireless keyboard battery";case TMI_AUTHOR:return L"chenyongxu250";case TMI_COPYRIGHT:return L"MIT";case TMI_VERSION:return L"0.1.0";case TMI_URL:return L"https://github.com/chenyongxu250/TrafficMonitor";default:return L"";}}
 ~Plugin(){running=false;if(worker.joinable())worker.join();}
};
Plugin plugin;
}
extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance(){return &plugin;}
