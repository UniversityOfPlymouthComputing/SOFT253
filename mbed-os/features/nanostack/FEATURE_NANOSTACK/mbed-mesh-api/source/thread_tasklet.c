/*
 * Copyright (c) 2015 ARM Limited. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h> //memset
#include "eventOS_event_timer.h"
#include "common_functions.h"
#include "net_interface.h"
#include "ip6string.h"  //ip6tos
#include "nsdynmemLIB.h"
#include "thread_management_if.h"
#include "net_polling_api.h"
#include "include/thread_tasklet.h"
#include "include/static_config.h"
#include "include/mesh_system.h"
#include <mbed_assert.h>
#include "ns_event_loop.h"

// For tracing we need to define flag, have include and define group
#define HAVE_DEBUG 1
#include "ns_trace.h"
#define TRACE_GROUP  "m6Thread"

#include "mac_api.h"
#include "sw_mac.h"

#define DETAILED_TRACES
#ifdef DETAILED_TRACES
#define TRACE_DETAIL    tr_debug
#else
#define TRACE_DETAIL(...)
#endif

#define INTERFACE_NAME   "6L-THREAD"

// Tasklet timer events
#define TIMER_EVENT_START_BOOTSTRAP   1

#define INVALID_INTERFACE_ID        (-1)

/*
 * Thread tasklet states.
 */
typedef enum {
    TASKLET_STATE_CREATED = 0,
    TASKLET_STATE_INITIALIZED,
    TASKLET_STATE_BOOTSTRAP_STARTED,
    TASKLET_STATE_BOOTSTRAP_FAILED,
    TASKLET_STATE_BOOTSTRAP_READY
} tasklet_state_t;

/*
 * Mesh tasklet data structure.
 */
typedef struct {
    void (*mesh_api_cb)(mesh_connection_status_t nwk_status);
    channel_list_s channel_list;
    tasklet_state_t tasklet_state;
    int8_t tasklet;

    net_6lowpan_mode_e operating_mode;
    int8_t nwk_if_id;
    link_configuration_s link_config;

    /** Default network ID*/
    uint8_t networkid[16];
    uint8_t extented_panid[8];
    uint32_t pan_id;
    uint32_t rfChannel;
    uint8_t scan_time;
    net_6lowpan_gp_address_mode_e address_mode;
} thread_tasklet_data_str_t;


/* Tasklet data */
static thread_tasklet_data_str_t *thread_tasklet_data_ptr = NULL;
static device_configuration_s device_configuration;

/* private function prototypes */
void thread_tasklet_main(arm_event_s *event);
void thread_tasklet_network_state_changed(mesh_connection_status_t status);
void thread_tasklet_parse_network_event(arm_event_s *event);
void thread_tasklet_configure_and_connect_to_network(void);
#define TRACE_THREAD_TASKLET
#ifndef TRACE_THREAD_TASKLET
#define thread_tasklet_trace_bootstrap_info() ((void) 0)
#else
void thread_tasklet_trace_bootstrap_info(void);
#endif

/*
 * \brief A function which will be eventually called by NanoStack OS when ever the OS has an event to deliver.
 * @param event, describes the sender, receiver and event type.
 *
 * NOTE: Interrupts requested by HW are possible during this function!
 */
void thread_tasklet_main(arm_event_s *event)
{
    arm_library_event_type_e event_type;
    event_type = (arm_library_event_type_e) event->event_type;

    switch (event_type) {
        case ARM_LIB_NWK_INTERFACE_EVENT:
            /* This event is delivered every and each time when there is new
             * information of network connectivity.
             */
            thread_tasklet_parse_network_event(event);
            break;

        case ARM_LIB_TASKLET_INIT_EVENT:
            /* Event with type EV_INIT is an initializer event of NanoStack OS.
             * The event is delivered when the NanoStack OS is running fine.
             * This event should be delivered ONLY ONCE.
             */
            mesh_system_send_connect_event(thread_tasklet_data_ptr->tasklet);
            break;

        case ARM_LIB_SYSTEM_TIMER_EVENT:
            eventOS_event_timer_cancel(event->event_id,
                                       thread_tasklet_data_ptr->tasklet);

            if (event->event_id == TIMER_EVENT_START_BOOTSTRAP) {
                tr_debug("Restart bootstrap");
                arm_nwk_interface_up(thread_tasklet_data_ptr->nwk_if_id);
            }
            break;

        case APPLICATION_EVENT:
            if (event->event_id == APPL_EVENT_CONNECT) {
                thread_tasklet_configure_and_connect_to_network();
            }
            break;

        default:
            break;
    } // switch(event_type)
}

/**
 * \brief Network state event handler.
 * \param event show network start response or current network state.
 *
 * - ARM_NWK_BOOTSTRAP_READY: Save NVK persistent data to NVM and Net role
 * - ARM_NWK_NWK_SCAN_FAIL: Link Layer Active Scan Fail, Stack is Already at Idle state
 * - ARM_NWK_IP_ADDRESS_ALLOCATION_FAIL: No ND Router at current Channel Stack is Already at Idle state
 * - ARM_NWK_NWK_CONNECTION_DOWN: Connection to Access point is lost wait for Scan Result
 * - ARM_NWK_NWK_PARENT_POLL_FAIL: Host should run net start without any PAN-id filter and all channels
 * - ARM_NWK_AUHTENTICATION_FAIL: Pana Authentication fail, Stack is Already at Idle state
 */
void thread_tasklet_parse_network_event(arm_event_s *event)
{
    arm_nwk_interface_status_type_e status = (arm_nwk_interface_status_type_e) event->event_data;
    tr_debug("app_parse_network_event() %d", status);
    switch (status) {
        case ARM_NWK_BOOTSTRAP_READY:
            /* Network is ready and node is connected to Access Point */
            if (thread_tasklet_data_ptr->tasklet_state != TASKLET_STATE_BOOTSTRAP_READY) {
                tr_info("Thread bootstrap ready");
                thread_tasklet_data_ptr->tasklet_state = TASKLET_STATE_BOOTSTRAP_READY;
                thread_tasklet_trace_bootstrap_info();
                thread_tasklet_network_state_changed(MESH_CONNECTED);
            }
            break;
        case ARM_NWK_NWK_SCAN_FAIL:
            /* Link Layer Active Scan Fail, Stack is Already at Idle state */
            tr_debug("Link Layer Scan Fail: No Beacons");
            thread_tasklet_data_ptr->tasklet_state = TASKLET_STATE_BOOTSTRAP_FAILED;
            break;
        case ARM_NWK_IP_ADDRESS_ALLOCATION_FAIL:
            /* No ND Router at current Channel Stack is Already at Idle state */
            tr_debug("ND Scan/ GP REG fail");
            thread_tasklet_data_ptr->tasklet_state = TASKLET_STATE_BOOTSTRAP_FAILED;
            break;
        case ARM_NWK_NWK_CONNECTION_DOWN:
            /* Connection to Access point is lost wait for Scan Result */
            tr_debug("ND/RPL scan new network");
            thread_tasklet_data_ptr->tasklet_state = TASKLET_STATE_BOOTSTRAP_FAILED;
            break;
        case ARM_NWK_NWK_PARENT_POLL_FAIL:
            thread_tasklet_data_ptr->tasklet_state = TASKLET_STATE_BOOTSTRAP_FAILED;
            break;
        case ARM_NWK_AUHTENTICATION_FAIL:
            tr_debug("Network authentication fail");
            thread_tasklet_data_ptr->tasklet_state = TASKLET_STATE_BOOTSTRAP_FAILED;
            break;
        default:
            tr_warn("Unknown event %d", status);
            break;
    }

    if (thread_tasklet_data_ptr->tasklet_state != TASKLET_STATE_BOOTSTRAP_READY) {
        // Set 5s timer for a new network scan
        eventOS_event_timer_request(TIMER_EVENT_START_BOOTSTRAP,
                                    ARM_LIB_SYSTEM_TIMER_EVENT,
                                    thread_tasklet_data_ptr->tasklet,
                                    5000);
    }
}

/*
 * \brief Configure mesh network
 *
 */
void thread_tasklet_configure_and_connect_to_network(void)
{
    int8_t status;

    if (MBED_MESH_API_THREAD_DEVICE_TYPE == MESH_DEVICE_TYPE_THREAD_SLEEPY_END_DEVICE) {
        thread_tasklet_data_ptr->operating_mode = NET_6LOWPAN_SLEEPY_HOST;
    } else {
        thread_tasklet_data_ptr->operating_mode = NET_6LOWPAN_ROUTER;
    }

    arm_nwk_interface_configure_6lowpan_bootstrap_set(
        thread_tasklet_data_ptr->nwk_if_id,
        thread_tasklet_data_ptr->operating_mode,
        NET_6LOWPAN_THREAD);

    // Link configuration
    thread_tasklet_data_ptr->link_config.panId = MBED_MESH_API_THREAD_CONFIG_PANID;
    TRACE_DETAIL("PANID %x", thread_tasklet_data_ptr->link_config.panId);

    thread_tasklet_data_ptr->link_config.rfChannel = MBED_MESH_API_THREAD_CONFIG_CHANNEL;
    thread_tasklet_data_ptr->channel_list.channel_page = (channel_page_e)MBED_MESH_API_THREAD_CONFIG_CHANNEL_PAGE;
    thread_tasklet_data_ptr->channel_list.channel_mask[0] = MBED_MESH_API_THREAD_CONFIG_CHANNEL_MASK;
    TRACE_DETAIL("channel: %d", thread_tasklet_data_ptr->link_config.rfChannel);
    TRACE_DETAIL("channel page: %d", thread_tasklet_data_ptr->channel_list.channel_page);
    TRACE_DETAIL("channel mask: %d", (int)thread_tasklet_data_ptr->channel_list.channel_mask[0]);
    
    // Beacon data setting
    thread_tasklet_data_ptr->link_config.Protocol_id = 0x03;
    thread_tasklet_data_ptr->link_config.version = 1;
    memcpy(thread_tasklet_data_ptr->link_config.extended_random_mac, device_configuration.eui64, 8);
    thread_tasklet_data_ptr->link_config.extended_random_mac[0] |= 0x02;

    // network name
    MBED_ASSERT(strlen(MBED_MESH_API_THREAD_CONFIG_NETWORK_NAME) > 0 && strlen(MBED_MESH_API_THREAD_CONFIG_NETWORK_NAME) < 17);
    memcpy(thread_tasklet_data_ptr->link_config.name, MBED_MESH_API_THREAD_CONFIG_NETWORK_NAME, strlen(MBED_MESH_API_THREAD_CONFIG_NETWORK_NAME));       
    
    thread_tasklet_data_ptr->link_config.timestamp = MBED_MESH_API_THREAD_CONFIG_COMMISSIONING_DATASET_TIMESTAMP;
    
    // extended pan-id
    const uint8_t extented_panid[] = MBED_MESH_API_THREAD_CONFIG_EXTENDED_PANID;
    MBED_ASSERT(sizeof(extented_panid) == 8);        
    memcpy(thread_tasklet_data_ptr->link_config.extented_pan_id, extented_panid, sizeof(extented_panid));
    
    // Mesh prefix
    const uint8_t mesh_local_prefix[] = MBED_MESH_API_THREAD_CONFIG_ML_PREFIX;
    MBED_ASSERT(sizeof(mesh_local_prefix) == 8);   
    
    memcpy(thread_tasklet_data_ptr->link_config.mesh_local_ula_prefix, mesh_local_prefix, 8);
    TRACE_DETAIL("Mesh prefix: %s", trace_array(mesh_local_prefix, 8));   

    // Master Key
    const uint8_t master_key[] = MBED_MESH_API_THREAD_MASTER_KEY;
    MBED_ASSERT(sizeof(master_key) == 16);
    memcpy(thread_tasklet_data_ptr->link_config.master_key, master_key, 16);

    // PSKc
    const uint8_t PSKc[] = MBED_MESH_API_THREAD_CONFIG_PSKC;
    MBED_ASSERT(sizeof(PSKc) == 16);
    memcpy(thread_tasklet_data_ptr->link_config.PSKc, PSKc, 16);

    // PSKd
    const char PSKd[] = MBED_MESH_API_THREAD_PSKD;
    MBED_ASSERT(sizeof(PSKd) > 5 && sizeof(PSKd) < 33);         

    char *dyn_buf = ns_dyn_mem_alloc(sizeof(PSKd));
    strcpy(dyn_buf, PSKd);
    ns_dyn_mem_free(device_configuration.PSKd_ptr);
    device_configuration.PSKd_ptr = (uint8_t*)dyn_buf;
    device_configuration.PSKd_len = sizeof(PSKd) - 1;

    thread_tasklet_data_ptr->link_config.key_rotation = 3600;
    thread_tasklet_data_ptr->link_config.key_sequence = 0;

    thread_management_node_init(thread_tasklet_data_ptr->nwk_if_id,
                               &thread_tasklet_data_ptr->channel_list,
                               &device_configuration,
                               &thread_tasklet_data_ptr->link_config);

    status = arm_nwk_interface_up(thread_tasklet_data_ptr->nwk_if_id);

    if (status >= 0) {
        thread_tasklet_data_ptr->tasklet_state = TASKLET_STATE_BOOTSTRAP_STARTED;
        tr_info("Start Thread bootstrap (%s mode)", thread_tasklet_data_ptr->operating_mode == NET_6LOWPAN_SLEEPY_HOST ? "SED" : "Router");
    } else {
        thread_tasklet_data_ptr->tasklet_state = TASKLET_STATE_BOOTSTRAP_FAILED;
        tr_err("Bootstrap start failed, %d", status);
        thread_tasklet_network_state_changed(MESH_BOOTSTRAP_START_FAILED);
    }
}

/*
 * Inform application about network state change
 */
void thread_tasklet_network_state_changed(mesh_connection_status_t status)
{
    if (thread_tasklet_data_ptr->mesh_api_cb) {
        (thread_tasklet_data_ptr->mesh_api_cb)(status);
    }
}

/*
 * Trace bootstrap information.
 */
#ifdef TRACE_THREAD_TASKLET
void thread_tasklet_trace_bootstrap_info()
{
    link_layer_address_s app_link_address_info;
    uint8_t temp_ipv6[16];
    if (arm_net_address_get(thread_tasklet_data_ptr->nwk_if_id,
                            ADDR_IPV6_GP, temp_ipv6) == 0) {
        tr_debug("GP IPv6: %s", trace_ipv6(temp_ipv6));
    }

    if (arm_nwk_mac_address_read(thread_tasklet_data_ptr->nwk_if_id,
                                 &app_link_address_info) != 0) {
        tr_error("MAC Address read fail\n");
    } else {
        uint8_t temp[2];
        common_write_16_bit(app_link_address_info.mac_short,temp);
        tr_debug("MAC 16-bit: %s", trace_array(temp, 2));
        common_write_16_bit(app_link_address_info.PANId, temp);
        tr_debug("PAN ID: %s", trace_array(temp, 2));
        tr_debug("MAC 64-bit: %s", trace_array(app_link_address_info.mac_long, 8));
        tr_debug("IID (Based on MAC 64-bit address): %s", trace_array(app_link_address_info.iid_eui64, 8));
    }
}
#endif /* #define TRACE_THREAD_TASKLET */

int8_t thread_tasklet_get_ip_address(char *address, int8_t len)
{
    uint8_t binary_ipv6[16];

    if ((len >= 40) && (0 == arm_net_address_get(
                            thread_tasklet_data_ptr->nwk_if_id, ADDR_IPV6_GP, binary_ipv6))) {
        ip6tos(binary_ipv6, address);
        //tr_debug("IP address: %s", address);
        return 0;
    } else {
        return -1;
    }
}

int8_t thread_tasklet_connect(mesh_interface_cb callback, int8_t nwk_interface_id)
{
    int8_t re_connecting = true;
    int8_t tasklet = thread_tasklet_data_ptr->tasklet;

    if (thread_tasklet_data_ptr->nwk_if_id != INVALID_INTERFACE_ID) {
        return -3;  // already connected to network
    }

    if (thread_tasklet_data_ptr->tasklet_state == TASKLET_STATE_CREATED) {
        re_connecting = false;
    }

    memset(thread_tasklet_data_ptr, 0, sizeof(thread_tasklet_data_str_t));
    thread_tasklet_data_ptr->mesh_api_cb = callback;
    thread_tasklet_data_ptr->nwk_if_id = nwk_interface_id;
    thread_tasklet_data_ptr->tasklet_state = TASKLET_STATE_INITIALIZED;

    if (re_connecting == false) {
        thread_tasklet_data_ptr->tasklet = eventOS_event_handler_create(&thread_tasklet_main,
                        ARM_LIB_TASKLET_INIT_EVENT);
        if (thread_tasklet_data_ptr->tasklet < 0) {
            // -1 handler already used by other tasklet
            // -2 memory allocation failure
            return thread_tasklet_data_ptr->tasklet;
        }
        ns_event_loop_thread_start();
    } else {
        thread_tasklet_data_ptr->tasklet = tasklet;
        mesh_system_send_connect_event(thread_tasklet_data_ptr->tasklet);
    }

    return thread_tasklet_data_ptr->tasklet;
}

int8_t thread_tasklet_disconnect(bool send_cb)
{
    int8_t status = -1;
    // check that module is initialized
    if (thread_tasklet_data_ptr != NULL) {
        if (thread_tasklet_data_ptr->nwk_if_id != INVALID_INTERFACE_ID) {
            status = arm_nwk_interface_down(thread_tasklet_data_ptr->nwk_if_id);
            thread_tasklet_data_ptr->nwk_if_id = INVALID_INTERFACE_ID;
            if (send_cb == true) {
                thread_tasklet_network_state_changed(MESH_DISCONNECTED);
            }
        }

        // Clear callback, it will be set again in next connect
        thread_tasklet_data_ptr->mesh_api_cb = NULL;
    }
    return status;
}

void thread_tasklet_init(void)
{
    if (thread_tasklet_data_ptr == NULL) {
        thread_tasklet_data_ptr = ns_dyn_mem_alloc(sizeof(thread_tasklet_data_str_t));
        thread_tasklet_data_ptr->tasklet_state = TASKLET_STATE_CREATED;
        thread_tasklet_data_ptr->nwk_if_id = INVALID_INTERFACE_ID;
    }
}

int8_t thread_tasklet_network_init(int8_t device_id)
{
    // TODO, read interface name from configuration
    mac_description_storage_size_t storage_sizes;
    storage_sizes.device_decription_table_size = 32;
    storage_sizes.key_description_table_size = 6;
    storage_sizes.key_lookup_size = 1;
    storage_sizes.key_usage_size = 3;
    mac_api_t *api = ns_sw_mac_create(device_id, &storage_sizes);
    return arm_nwk_interface_lowpan_init(api, INTERFACE_NAME);
}

void thread_tasklet_device_config_set(uint8_t *eui64, char *pskd)
{
    (void) pskd; // this parameter is delivered via mbed configuration
    memcpy(device_configuration.eui64, eui64, 8);
}

int8_t thread_tasklet_data_poll_rate_set(uint32_t timeout)
{
    int8_t status = -1;
    if (thread_tasklet_data_ptr) {
        if (timeout != 0) {
            status = arm_nwk_host_mode_set(thread_tasklet_data_ptr->nwk_if_id, NET_HOST_SLOW_POLL_MODE, timeout);
        } else {
            status = arm_nwk_host_mode_set(thread_tasklet_data_ptr->nwk_if_id, NET_HOST_RX_ON_IDLE, timeout);
        }
    }

    return status;
}

