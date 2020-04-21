#ifndef LIMESDR_FEI_I_IMPL_H
#define LIMESDR_FEI_I_IMPL_H

#include "LimeSDR_FEI_base.h"
#include "lime/LimeSuite.h"
#include <boost/lexical_cast.hpp>
#include <math.h>

/*********************************************************************************************/
/**************************        Multi Process Thread Class       **************************/
/*********************************************************************************************/
/** Note:: This class is based off of the process thread class in the USRP_base.h file.      */
/**             Changed to accept serviceFunction as argument, rather than hard coded        */
/**             Added interrupt() member function to interrupt underlying boost::thread      */
/*********************************************************************************************/
template < typename TargetClass >
class MultiProcessThread
{
public:
    MultiProcessThread(TargetClass *_target, int (TargetClass::*_func)(),float _delay)
    {
        service_function = boost::bind(_func, _target);
        _mythread = 0;
        _thread_running = false;
        _udelay = (__useconds_t)(_delay * 1000000);
    };

    // kick off the thread
    void start() {
        if (_mythread == 0) {
            _thread_running = true;
            _mythread = new boost::thread(&MultiProcessThread::run, this);
        }
    };

    // manage calls to target's service function
    void run() {
        int state = NORMAL;
        while (_thread_running and (state != FINISH)) {
            state = service_function();
            if (state == NOOP) usleep(_udelay);
        }
    };

    // stop thread and wait for termination
    bool release(unsigned long secs = 0, unsigned long usecs = 0) {
        _thread_running = false;
        if (_mythread)  {
            if ((secs == 0) and (usecs == 0)){
                _mythread->join();
            } else {
                boost::system_time waitime= boost::get_system_time() + boost::posix_time::seconds(secs) +  boost::posix_time::microseconds(usecs) ;
                if (!_mythread->timed_join(waitime)) {
                    return 0;
                }
            }
            delete _mythread;
            _mythread = 0;
        }

        return 1;
    };

    virtual ~MultiProcessThread(){
        if (_mythread != 0) {
            release(0);
            _mythread = 0;
        }
    };

    void updateDelay(float _delay) { _udelay = (__useconds_t)(_delay * 1000000); };

private:
    boost::thread *_mythread;
    bool _thread_running;
    boost::function<int ()> service_function;
    __useconds_t _udelay;
    boost::condition_variable _end_of_run;
    boost::mutex _eor_mutex;
};

typedef struct ticket_lock {
    ticket_lock(){
        cond=NULL;
        mutex=NULL;
        queue_head=queue_tail=0;
    }
    boost::condition_variable* cond;
    boost::mutex* mutex;
    size_t queue_head, queue_tail;
} ticket_lock_t;

class scoped_tuner_lock{
    public:
        scoped_tuner_lock(ticket_lock_t& _ticket){
            ticket = &_ticket;

            boost::mutex::scoped_lock lock(*ticket->mutex);
            queue_me = ticket->queue_tail++;
            while (queue_me != ticket->queue_head)
            {
                ticket->cond->wait(lock);
            }
        }
        ~scoped_tuner_lock(){
            boost::mutex::scoped_lock lock(*ticket->mutex);
            ticket->queue_head++;
            ticket->cond->notify_all();
        }
    private:
        ticket_lock_t* ticket;
        size_t queue_me;
};

class LimeSDR_FEI_i : public LimeSDR_FEI_base
{
    ENABLE_LOGGING
    public:
        LimeSDR_FEI_i(char *devMgr_ior, char *id, char *lbl, char *sftwrPrfl);
        LimeSDR_FEI_i(char *devMgr_ior, char *id, char *lbl, char *sftwrPrfl, char *compDev);
        LimeSDR_FEI_i(char *devMgr_ior, char *id, char *lbl, char *sftwrPrfl, CF::Properties capacities);
        LimeSDR_FEI_i(char *devMgr_ior, char *id, char *lbl, char *sftwrPrfl, CF::Properties capacities, char *compDev);
        ~LimeSDR_FEI_i();

        void constructor();

        int serviceFunction(){ return FINISH; }
        int serviceFunctionReceive();
        int serviceFunctionTransmit();
        void start() throw (CF::Resource::StartError, CORBA::SystemException);
        void stop() throw (CF::Resource::StopError, CORBA::SystemException);

    protected:
        std::string getTunerType(const std::string& allocation_id);
        bool getTunerDeviceControl(const std::string& allocation_id);
        std::string getTunerGroupId(const std::string& allocation_id);
        std::string getTunerRfFlowId(const std::string& allocation_id);
        double getTunerCenterFrequency(const std::string& allocation_id);
        void setTunerCenterFrequency(const std::string& allocation_id, double freq);
        double getTunerBandwidth(const std::string& allocation_id);
        void setTunerBandwidth(const std::string& allocation_id, double bw);
        bool getTunerAgcEnable(const std::string& allocation_id);
        void setTunerAgcEnable(const std::string& allocation_id, bool enable);
        float getTunerGain(const std::string& allocation_id);
        void setTunerGain(const std::string& allocation_id, float gain);
        long getTunerReferenceSource(const std::string& allocation_id);
        void setTunerReferenceSource(const std::string& allocation_id, long source);
        bool getTunerEnable(const std::string& allocation_id);
        void setTunerEnable(const std::string& allocation_id, bool enable);
        double getTunerOutputSampleRate(const std::string& allocation_id);
        void setTunerOutputSampleRate(const std::string& allocation_id, double sr);
        std::string get_rf_flow_id(const std::string& port_name);
        void set_rf_flow_id(const std::string& port_name, const std::string& id);
        frontend::RFInfoPkt get_rfinfo_pkt(const std::string& port_name);
        void set_rfinfo_pkt(const std::string& port_name, const frontend::RFInfoPkt& pkt);

    private:
        ////////////////////////////////////////
        // Required device specific functions // -- to be implemented by device developer
        ////////////////////////////////////////

        // these are pure virtual, must be implemented here
        void deviceEnable(frontend_tuner_status_struct_struct &fts, size_t tuner_id);
        void deviceDisable(frontend_tuner_status_struct_struct &fts, size_t tuner_id);
        bool deviceSetTuning(const frontend::frontend_tuner_allocation_struct &request, frontend_tuner_status_struct_struct &fts, size_t tuner_id);
        bool deviceDeleteTuning(frontend_tuner_status_struct_struct &fts, size_t tuner_id);

        void getChannelProperties(int num_channels, bool transmit);
        void getChannelStatus(int channel, bool transmit);
        void getAdvancedControlStatus(int channel, bool transmit);
        void allocateLimeSDR(int channel, bool transmit, double freq, double sample_rate, int oversample_ratio, double bandwidth, double gain);
        void Error(std::string err);
        bulkio::OutFloatStream outputStream;

        // serviceFunctionTransmit thread
        MultiProcessThread<LimeSDR_FEI_i> *receive_service_thread;
        MultiProcessThread<LimeSDR_FEI_i> *transmit_service_thread;
        boost::mutex receive_service_thread_lock;
        boost::mutex transmit_service_thread_lock;
        template <class IN_PORT_TYPE> bool transmitHelper(IN_PORT_TYPE *dataIn);

};

#endif // LIMESDR_FEI_I_IMPL_H
