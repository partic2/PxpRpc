/* pxprpc runtime bridge is mainly about communicate between runtime in one process. 
   
   WARNING: These APIs are related the language implemention, So they are not so stable like others.

*/

#include <stdint.h>
#include <pxprpc.h>



#define pxprpc_rtbridge_event_id_accept 1
#define pxprpc_rtbridge_event_id_connected 2
#define pxprpc_rtbridge_event_id_receiveDone 3
#define pxprpc_rtbridge_event_id_sendDone 4

#define pxprpc_rtbridge_event_id_serve 0x11
#define pxprpc_rtbridge_event_id_connect 0x12
#define pxprpc_rtbridge_event_id_receive 0x13
#define pxprpc_rtbridge_event_id_send 0x14
#define pxprpc_rtbridge_event_id_close 0x15

typedef void *pxprpc_rtbridge_context;

/* related the others language parser, So we layout it manually.
   assert sizeof(void *)<=8
 */
#pragma pack(1)
typedef struct pxprpc_rtbridge_event_s{
    uint32_t eventSize;
    uint16_t id;
    /* once set to 1, the consumer should never this event again 
       and provider can free this event buffer.
       Specially NOW "serve" invocation still use the this event, so provider 
       start the server can only free this buffer after server stopped. */
    uint8_t processed;
    uint8_t padding1;
    union{
        pxprpc_rtbridge_context context;
        uint64_t padding2;
    };
    union{
        struct pxprpc_rtbridge_event_s *next;
        uint64_t padding3;
    };
    /* offset=24 */
    union{
        struct{
            uint64_t connId; 
            uint64_t p;
        }accept;
        struct{
            uint64_t connId;
            uint64_t p;
        }connected;
        struct{
            uint64_t connId;
            uint32_t len;
        }receiveDone;
        struct{
            uint64_t connId;
            uint64_t p;
        }sendDone;
        struct{
            uint64_t serveId;
            uint64_t p;
            uint8_t startOrStop;
        } serve;
        struct{
            uint64_t p;
        } connect;
        struct{
            uint64_t connId;
            union{
                struct pxprpc_buffer_part buf;
                uint64_t alignPadding3[4];
            };
        } receive;
        struct {
            uint64_t connId;
            uint64_t p;
            uint32_t dataSize;
            /* use for store temp buf struct by native side, depend on sizeof(struct pxprpc_buffer_part) */
            union{
                struct pxprpc_buffer_part buf;
                uint64_t alignPadding3[4];
            };
        } send;
        struct {
            uint64_t connId;
        } close;
        char padding2[56];
    } p2;
    /* remain data for event offset=80 
    name for "serve" and "connect" or data for "receiveDone","send" */
    uint8_t p3[1];
};

#pragma pack()


pxprpc_rtbridge_context pxprpc_rtbridge_alloc();

void pxprpc_rtbridge_free(pxprpc_rtbridge_context c);

/* cross thread model, one thread block and wait for result, another push invocation */
struct pxprpc_rtbridge_event_s *pxprpc_rtbridge_pull_result(pxprpc_rtbridge_context context);

void pxprpc_rtbridge_push_invoc(pxprpc_rtbridge_context context,struct pxprpc_rtbridge_event_s *);
