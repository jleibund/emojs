/* This code is PUBLIC DOMAIN, and is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND. See the accompanying 
 * LICENSE file.
 */

#include <v8.h>
#include <node.h>



using namespace node;
using namespace v8;

#define REQ_FUN_ARG(I, VAR)                                             \
if (args.Length() <= (I) || !args[I]->IsFunction())                   \
return ThrowException(Exception::TypeError(                         \
String::New("Argument " #I " must be a function")));  \
Local<Function> VAR = Local<Function>::Cast(args[I]);

class NodeEPOCDriver: ObjectWrap
{
private:
  int m_count;
    int connected;
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
    m_count(0), connected(0)
  {
      loop = uv_default_loop();
  }

  ~NodeEPOCDriver()
  {
  }
    struct baton_t {
        NodeEPOCDriver *hw;
    };

  static Handle<Value> New(const Arguments& args)
  {
    HandleScope scope;
    NodeEPOCDriver* hw = new NodeEPOCDriver();
    hw->Wrap(args.This());
    return args.This();
  }

    static Handle<Value> connect(const Arguments& args){

        REQ_FUN_ARG(0, cb);

        NodeEPOCDriver* hw = ObjectWrap::Unwrap<NodeEPOCDriver>(args.This());
        if (hw->connected == 0){
            printf("connected to epoc event loop\n");
            hw->cb = Persistent<Function>::New(cb);
            hw->timer.data = hw;
            uv_timer_init(hw->loop, &hw->timer);
            uv_timer_start(&hw->timer, &hw->timer_cb,50,50);
            hw->connected = 1;
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
            printf("disconnected from epoc event loop\n");
            uv_timer_stop(&hw->timer);
            uv_unref((uv_handle_t*)hw->loop);
            hw->connected = 0;
            hw->cb.Dispose();
        }
        return Undefined();
    }

    
    static void process(uv_work_t* req)
    {
        //baton_t *baton = static_cast<baton_t *>(req->data);
        // this is where we need to poll for the next event
    }
    
    static void after_process(uv_work_t* req) {
        HandleScope scope;
        baton_t *baton = static_cast<baton_t *>(req->data);
        baton->hw->Unref();
        
        Local<Value> argv[1];
        
        argv[0] = String::New("Hello World");
        
        TryCatch try_catch;
        
        baton->hw->cb->Call(Context::GetCurrent()->Global(), 1, argv);
        
        if (try_catch.HasCaught()) {
            FatalException(try_catch);
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
