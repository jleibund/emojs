/* This code is PUBLIC DOMAIN, and is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND. See the accompanying 
 * LICENSE file.
 */

#include <v8.h>
#include <node.h>
#include <sstream>
#include <fstream>
#include <iostream>
#include <map>
#include "edk.h"
#include "edkErrorCode.h"
#include "EmoStateDLL.h"

using namespace node;
using namespace v8;
using namespace std;

#define REQ_FUN_ARG(I, VAR)                                             \
if (args.Length() <= (I) || !args[I]->IsFunction())                   \
return ThrowException(Exception::TypeError(                         \
String::New("Argument " #I " must be a function")));  \
Local<Function> VAR = Local<Function>::Cast(args[I]);

#define REQ_STRING_ARG(I, VAR)                                             \
if (args.Length() <= (I) || !args[I]->IsString())                   \
return ThrowException(Exception::TypeError(                         \
String::New("Argument " #I " must be a string")));  \
String::Utf8Value VAR(args[I]->ToString());

class NodeEPOCDriver: ObjectWrap
{
private:
  int connected;
  unsigned int userID;
  unsigned short composerPort;
  int option;
  char * profile;
  uv_loop_t* loop;
  uv_timer_t timer;
  Persistent<Function> cb;
public:

  static Persistent<FunctionTemplate> s_ct;
  static void Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("NodeEPOCDriver"));

      NODE_SET_PROTOTYPE_METHOD(s_ct, "connect", connect);
      NODE_SET_PROTOTYPE_METHOD(s_ct, "disconnect", disconnect);

    target->Set(String::NewSymbol("NodeEPOCDriver"),
                s_ct->GetFunction());
  }

  NodeEPOCDriver() :
    connected(0), userID(0), composerPort(1726), option(0)
  {
      loop = uv_default_loop();
  }

  ~NodeEPOCDriver()
  {
  }
    struct baton_t {
        NodeEPOCDriver *hw;
        bool update;
        int cog;
        float cog_power;
        int blink;
        int winkLeft;
        int winkRight;
        int lookLeft;
        int lookRight;
        float timestamp;
    };

  static Handle<Value> New(const Arguments& args)
  {
    HandleScope scope;
    NodeEPOCDriver* hw = new NodeEPOCDriver();
    hw->Wrap(args.This());
    return args.This();
  }

    static Handle<Value> connect(const Arguments& args){

        NodeEPOCDriver* hw = ObjectWrap::Unwrap<NodeEPOCDriver>(args.This());

        // convert it to string

        REQ_STRING_ARG(0,param0);
        REQ_FUN_ARG(1, cb);

        string profileFile = string(*param0);

        // read arg for option, engine or remote..
        // read option for profile
        if (hw->connected == 0){

            bool connect = false;
            switch(hw->option){
                case 0: {
                    connect= (EE_EngineConnect() == EDK_OK);
                    break;
                }
                case 1: {
                    connect= (EE_EngineRemoteConnect("127.0.0.1",1726)==EDK_OK);
                    break;
                }
                default: {break;}
            }

            if (connect){
                // read in the profile
                ifstream::pos_type size;
                ifstream file (profileFile.c_str());
                if (file.is_open()){
                    size = file.tellg();
                    hw->profile = new char [size];
                    file.seekg (0, ios::beg);
                    file.read (hw->profile, size);
                    file.close();

                    EE_SetUserProfile(hw->userID, (unsigned char*)hw->profile, (int)size);
                    cout << "opened profile: "<<profileFile << endl;

                } else {
                    cout << "could not open profile file:" <<profileFile <<endl;
                }

               // printf("connecting to epoc event loop: %i using profile %s\n",connect);
                hw->cb = Persistent<Function>::New(cb);
                hw->timer.data = hw;
                uv_timer_init(hw->loop, &hw->timer);
                uv_timer_start(&hw->timer, &hw->timer_cb,50,100);
                hw->connected = 1;
            } else {
                cout << "error connecting" << endl;
            }
        }
        return Undefined();
    }
    static void timer_cb(uv_timer_t* timer, int stat){

        NodeEPOCDriver* hw = static_cast<NodeEPOCDriver*>(timer->data);

        // create an event to throw on queue for processing..
        baton_t *baton = new baton_t();
        baton->hw = hw;
        uv_work_t *req = new uv_work_t();
        req->data = baton;
        hw->Ref();
        
        int status = uv_queue_work(hw->loop, req, process, after_process);
        assert(status == 0);

    }

    static Handle<Value> disconnect(const Arguments& args){
        NodeEPOCDriver* hw = ObjectWrap::Unwrap<NodeEPOCDriver>(args.This());
        if (hw->connected == 1){
            cout << "disconnected from epoc event loop" << endl;
            uv_timer_stop(&hw->timer);
            uv_unref((uv_handle_t*)hw->loop);
            if (hw->profile)
                delete[] hw->profile;
            hw->connected = 0;
            hw->cb.Dispose();
            EE_EngineDisconnect();
        }
        return Undefined();
    }

    
    static void process(uv_work_t* req)
    {
        baton_t *baton = static_cast<baton_t *>(req->data);
        NodeEPOCDriver* hw = baton->hw;
        baton->update = false;
        if (hw->connected == 1){
            EmoEngineEventHandle eEvent = EE_EmoEngineEventCreate();
            EmoStateHandle eState = EE_EmoStateCreate();
            int state = EE_EngineGetNextEvent(eEvent);
            if (state == EDK_OK) {
                 EE_Event_t eventType = EE_EmoEngineEventGetType(eEvent);
                 EE_EmoEngineEventGetUserId(eEvent, &hw->userID);

                if (eventType == EE_EmoStateUpdated){
                    EE_EmoEngineEventGetEmoState(eEvent, eState);
                    baton->timestamp = ES_GetTimeFromStart(eState);
                    baton->cog = static_cast<int>(ES_CognitivGetCurrentAction(eState));
                    baton->cog_power = ES_CognitivGetCurrentActionPower(eState);
                    baton->blink = ES_ExpressivIsBlink(eState);
                    baton->winkLeft = ES_ExpressivIsLeftWink(eState);
                    baton->winkRight = ES_ExpressivIsRightWink(eState);
                    baton->lookLeft = ES_ExpressivIsLookingLeft(eState);
                    baton->lookRight = ES_ExpressivIsLookingRight(eState);
                    baton->update = true;
                }
            }
            EE_EmoStateFree(eState);
            EE_EmoEngineEventFree(eEvent);
        }
        // this is where we need to poll for the next event
    }
    
    static void after_process(uv_work_t* req) {
        HandleScope scope;
        baton_t *baton = static_cast<baton_t *>(req->data);
        baton->hw->Unref();


        Local<Value> argv[1];

        Local<Object> obj = Object::New();

        if (baton->update){

            // these will be the fields from the event
            obj->Set(String::NewSymbol("time"), Number::New(baton->timestamp));
            obj->Set(String::NewSymbol("userId"), Number::New(baton->hw->userID));
            obj->Set(String::NewSymbol("wirelessSignalStatus"),  Number::New(2));
            obj->Set(String::NewSymbol("blink"),  Number::New(baton->blink));
            obj->Set(String::NewSymbol("winkLeft"),  Number::New(baton->winkLeft));
            obj->Set(String::NewSymbol("winkRight"),  Number::New(baton->winkRight));
            obj->Set(String::NewSymbol("lookLeft"),  Number::New(baton->lookLeft));
            obj->Set(String::NewSymbol("lookRight"),  Number::New(baton->lookRight));
    //        obj->Set(String::NewSymbol("eyebrow"), Number::New(0.10f));
    //        obj->Set(String::NewSymbol("furrow"),  Number::New(0.10f));
    //        obj->Set(String::NewSymbol("smile"),  Number::New(0.10f));
    //        obj->Set(String::NewSymbol("clench"),  Number::New(0.10f));
    //        obj->Set(String::NewSymbol("smirkLeft"),  Number::New(0.10f));
    //        obj->Set(String::NewSymbol("smirkRight"),  Number::New(0.10f));
    //        obj->Set(String::NewSymbol("laugh"),  Number::New(0.10f));
    //        obj->Set(String::NewSymbol("shortTermExcitement"),  Number::New(0.10f));
    //        obj->Set(String::NewSymbol("longTermExcitement"),  Number::New(0.10f));
    //        obj->Set(String::NewSymbol("engagementOrBoredom"),  Number::New(0.10f));
            obj->Set(String::NewSymbol("cognitivAction"),  Number::New(baton->cog));
            obj->Set(String::NewSymbol("cognitivPower"),  Number::New(baton->cog_power));

            argv[0] = obj;

            TryCatch try_catch;

            baton->hw->cb->Call(Context::GetCurrent()->Global(), 1, argv);

            if (try_catch.HasCaught()) {
                FatalException(try_catch);
            }
        }

        delete baton;
    }

};

Persistent<FunctionTemplate> NodeEPOCDriver::s_ct;

extern "C" {
  static void init (Handle<Object> target)
  {
    NodeEPOCDriver::Init(target);
  }

  NODE_MODULE(emojs, init);
}
