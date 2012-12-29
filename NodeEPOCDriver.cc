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
  int run;
  uv_loop_t* loop;
  uv_loop_t* work;
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
    connected(0), userID(0), run(0)
  {
      work = uv_loop_new();
      loop = uv_default_loop();
  }

  ~NodeEPOCDriver()
  {
  }

    struct baton_t {
        NodeEPOCDriver *hw;
        EmoEngineEventHandle* eEvent;
        EmoStateHandle* eState;
        bool update;
        int cog;
        float cog_power;
        int blink;
        int winkLeft;
        int winkRight;
        int lookLeft;
        int lookRight;
        float eyebrow;
        float furrow;
        float smile;
        float clench;
        float smirkLeft;
        float smirkRight;
        float laugh;
        float shortTermExcitement;
        float longTermExcitement;
        float engagementOrBoredom;
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

        REQ_STRING_ARG(0,param0);
        REQ_FUN_ARG(1, cb);

        string profileFile = string(*param0);

        int option = 0;
        // get the option for engine or remote connect
        if (args.Length()==3 && args[2]->IsInt32()){
            Local<Integer> i = Local<Integer>::Cast(args[2]);
            int selected = (int)(i->Int32Value());
            if (selected != 0){
                option = 1;
            }
        }

        // read arg for option, engine or remote..
        // read option for profile
        if (hw->connected == 0){

            bool connect = false;
            switch(option){
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
                int error = EE_LoadUserProfile(hw->userID, profileFile.c_str());
                if (error == 0){
                    cout << "opened profile: "<<profileFile << endl;

                    unsigned long activeActions = 0;
                    EE_CognitivGetActiveActions(hw->userID, &activeActions);

                    cout << "active actions: " << activeActions << endl;

                } else {
                    cout << "error: "<< error << " could not open profile:" <<profileFile <<endl;
                }

                hw->cb = Persistent<Function>::New(cb);
                uv_work_t *req = new uv_work_t();
                req->data = hw;
                hw->run=1;

                int status = uv_queue_work(hw->loop, req, work_cb, after_work);
                assert(status == 0);

                cout << "starting epoc event loop" <<endl;

                hw->connected = 1;

            } else {
                cout << "error connecting" << endl;
            }
        }
        return Undefined();
    }
    static void work_cb(uv_work_t* req){

        NodeEPOCDriver *hw = static_cast<NodeEPOCDriver *>(req->data);
        while (true){
            if (hw->connected == 1){
                EmoEngineEventHandle eEvent = EE_EmoEngineEventCreate();
                EmoStateHandle eState = EE_EmoStateCreate();
                int state = EE_EngineGetNextEvent(eEvent);
                if (state == EDK_OK) {
                     EE_Event_t eventType = EE_EmoEngineEventGetType(eEvent);
                     EE_EmoEngineEventGetUserId(eEvent, &hw->userID);

                    if (eventType == EE_EmoStateUpdated){
                        EE_EmoEngineEventGetEmoState(eEvent, eState);

                        baton_t* baton = new baton_t();
                        baton->hw = hw;
                        baton->timestamp = ES_GetTimeFromStart(eState);
                        baton->cog = static_cast<int>(ES_CognitivGetCurrentAction(eState));
                        baton->cog_power = ES_CognitivGetCurrentActionPower(eState);
                        baton->blink = ES_ExpressivIsBlink(eState);
                        baton->winkLeft = ES_ExpressivIsLeftWink(eState);
                        baton->winkRight = ES_ExpressivIsRightWink(eState);
                        baton->lookLeft = ES_ExpressivIsLookingLeft(eState);
                        baton->lookRight = ES_ExpressivIsLookingRight(eState);
                        std::map<EE_ExpressivAlgo_t, float> expressivStates;
                        EE_ExpressivAlgo_t upperFaceAction = ES_ExpressivGetUpperFaceAction(eState);
                        float			   upperFacePower  = ES_ExpressivGetUpperFaceActionPower(eState);
                        EE_ExpressivAlgo_t lowerFaceAction = ES_ExpressivGetLowerFaceAction(eState);
                        float			   lowerFacePower  = ES_ExpressivGetLowerFaceActionPower(eState);
                        expressivStates[ upperFaceAction ] = upperFacePower;
                        expressivStates[ lowerFaceAction ] = lowerFacePower;
                        baton->eyebrow= expressivStates[ EXP_EYEBROW ];
                        baton->furrow= expressivStates[ EXP_FURROW ]; // furrow
                        baton->smile= expressivStates[ EXP_SMILE ]; // smile
                        baton->clench= expressivStates[ EXP_CLENCH ]; // clench
                        baton->smirkLeft= expressivStates[ EXP_SMIRK_LEFT  ]; // smirk left
                        baton->smirkRight= expressivStates[ EXP_SMIRK_RIGHT ]; // smirk right
                        baton->laugh= expressivStates[ EXP_LAUGH       ]; // laugh

                        // Affectiv Suite results
                        baton->shortTermExcitement= ES_AffectivGetExcitementShortTermScore(eState);
                        baton->longTermExcitement= ES_AffectivGetExcitementLongTermScore(eState);

                        baton->engagementOrBoredom= ES_AffectivGetEngagementBoredomScore(eState);

                        baton->update = true;

                        // create an event to throw on queue for processing..
                        uv_work_t *req2 = new uv_work_t();
                        req2->data = baton;

                        int status = uv_queue_work(hw->loop, req2, process, after_process);
                        assert(status == 0);
                     }
                }
                EE_EmoStateFree(eState);
                EE_EmoEngineEventFree(eEvent);
            }
        }
    }

    static void after_work(uv_work_t* req) {
    }

    static Handle<Value> disconnect(const Arguments& args){
        NodeEPOCDriver* hw = ObjectWrap::Unwrap<NodeEPOCDriver>(args.This());
        if (hw->connected == 1){
            cout << "disconnected from epoc event loop" << endl;

            hw->run=0;
            hw->connected = 0;
            hw->cb.Dispose();
            EE_EngineDisconnect();
        }
        return Undefined();
    }

    
    static void process(uv_work_t* req)
    {
    }
    
    static void after_process(uv_work_t* req) {
        HandleScope scope;
        baton_t *baton = static_cast<baton_t *>(req->data);

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
            obj->Set(String::NewSymbol("eyebrow"), Number::New(baton->eyebrow));
            obj->Set(String::NewSymbol("furrow"),  Number::New(baton->furrow));
            obj->Set(String::NewSymbol("smile"),  Number::New(baton->smile));
            obj->Set(String::NewSymbol("clench"),  Number::New(baton->clench));
            obj->Set(String::NewSymbol("smirkLeft"),  Number::New(baton->smirkLeft));
            obj->Set(String::NewSymbol("smirkRight"),  Number::New(baton->smirkRight));
            obj->Set(String::NewSymbol("laugh"),  Number::New(baton->laugh));
            obj->Set(String::NewSymbol("shortTermExcitement"),  Number::New(baton->shortTermExcitement));
            obj->Set(String::NewSymbol("longTermExcitement"),  Number::New(baton->longTermExcitement));
            obj->Set(String::NewSymbol("engagementOrBoredom"),  Number::New(baton->engagementOrBoredom));
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
