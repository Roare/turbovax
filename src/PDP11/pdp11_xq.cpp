/* pdp11_xq.c: DEQNA/DELQA ethernet controller simulator
  ------------------------------------------------------------------------------

   Copyright (c) 2002-2007, David T. Hittner

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   THE AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of the author shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from the author.

  ------------------------------------------------------------------------------

  This DEQNA/DELQA/DELQA-T simulation is based on:
    Digital DELQA Users Guide, Part# EK-DELQA-UG-002
    Digital DEQNA Users Guide, Part# EK-DEQNA-UG-001
    Digital DELQA-Plus Addendum to DELQA Users Guide, Part# EK-DELQP-UG-001_Sep89.pdf 
  These manuals can be found online at:
    http://www.bitsavers.org/pdf/dec/qbus

  Certain adaptations have been made because this is an emulation:
    Ethernet transceiver power flag CSR<12> is ON when attached.
    External Loopback does not go out to the physical adapter, it is
      implemented more like an extended Internal Loopback
    Time Domain Reflectometry (TDR) numbers are faked
    The 10-second approx. hardware/software reset delay does not exist
    Some physical ethernet receive events like Runts, Overruns, etc. are
      never reported back, since the packet-level driver never sees them

  Certain advantages are derived from this emulation:
    If the real ethernet controller is faster than 10Mbit/sec, the speed is
      seen by the simulated cpu since there are no minimum response times.

  Known Bugs or Unsupported features, in priority order:
    1) PDP11 bootstrap
    2) MOP functionality not implemented
    3) Local packet processing not implemented

  Regression Tests:
    VAX:    1. Console SHOW DEVICE
            2. VMS v7.2 boots/initializes/shows device
            3. VMS DECNET - SET HOST and COPY tests
            4. VMS MultiNet - SET HOST/TELNET and FTP tests
            5. VMS LAT - SET HOST/LAT tests
            6. VMS Cluster - SHOW CLUSTER, SHOW DEVICE, and cluster COPY tests
            7. Console boot into VMSCluster (>>>B XQAO)
            8. Console DELQA Diagnostic (>>>TEST 82)

    PDP11:  1. RT-11 v5.3 - FTPSB copy test
            2. RSTS/E v10.1 - detects/enables device

  ------------------------------------------------------------------------------

  Modification history:

  20-Apr-11  MP   Fixed missing information from save/restore which
                  caused operations to not complete correctly after 
                  a restore until the OS reset the controller.
  09-Dec-10  MP   Added address conflict check during attach.
  06-Dec-10  MP   Fixed loopback processing to correctly handle forward packets.
  29-Nov-10  MP   Fixed interrupt dispatch issue which caused delivered packets 
                  (in and out) to sometimes not interrupt the CPU after processing.
  07-Mar-08  MP   Fixed the SCP visibile SA registers to always display the 
                  ROM mac address, even after it is changed by SET XQ MAC=.
  07-Mar-08  MP   Added changes so that the Console DELQA diagnostic (>>>TEST 82) 
                  will succeed.
  03-Mar-08  MP   Added DELQA-T (aka DELQA Plus) device emulation support.
  06-Feb-08  MP   Added dropped frame statistics to record when the receiver discards
                  received packets due to the receiver being disabled, or due to the
                  XQ device's packet receive queue being full.
                  Fixed bug in receive processing when we're not polling.  This could
                  cause receive processing to never be activated again if we don't 
                  read all available packets via eth_read each time we get the 
                  opportunity.
  31-Jan-08  MP   Added the ability to Coalesce received packet interrupts.  This
                  is enabled by SET XQ POLL=DELAY=nnn where nnn is a number of 
                  microseconds to delay the triggering of an interrupt when a packet
                  is received.
  29-Jan-08  MP   Added SET XQ POLL=DISABLE (aka SET XQ POLL=0) to operate without 
                  polling for packet read completion.
  29-Jan-08  MP   Changed the sanity and id timer mechanisms to use a separate timer
                  unit so that transmit and recieve activities can be dealt with
                  by the normal xq_svc routine.
                  Dynamically determine the timer polling rate based on the 
                  calibrated tmr_poll and clk_tps values of the simulator.
  25-Jan-08  MP   Enabled the SET XQ POLL to be meaningful if the simulator currently
                  doesn't support idling.
  25-Jan-08  MP   Changed xq_debug_setup to use sim_debug instead of smp_printf so that
                  all debug output goes to the same place.
  25-Jan-08  MP   Restored the call to xq_svc after all successful calls to eth_write
                  to allow receive processing to happen before the next event
                  service time.  This must have been inadvertently commented out 
                  while other things were being tested.
  23-Jan-08  MP   Added debugging support to display packet headers and packet data
  18-Jun-07  RMS  Added UNIT_IDLE flag
  29-Oct-06  RMS  Synced poll and clock
  27-Jan-06  RMS  Fixed unaligned accesses in XQB (found by Doug Carman)
  07-Jan-06  RMS  Fixed unaligned access bugs (found by Doug Carman)
  07-Sep-05  DTH  Removed unused variable
  16-Aug-05  RMS  Fixed C++ declaration and cast problems
  01-Dec-04  DTH  Added runtime attach prompt
  27-Feb-04  DTH  Removed struct timeb deuggers
  31-Jan-04  DTH  Replaced #ifdef debuggers with inline debugging
  19-Jan-04  DTH  Combined service timers into one for efficiency
  16-Jan-04  DTH  Added more info to SHOW MOD commands, added SET/SHOW XQ DEBUG
  13-Jan-04  DTH  Corrected interrupt code with help from Tom Evans
  06-Jan-04  DTH  Added protection against changing mac and type if attached
  05-Jan-04  DTH  Moved most of xq_setmac to sim_ether
  26-Dec-03  DTH  Moved ethernet show and queue functions to sim_ether
  03-Dec-03  DTH  Added minimum name length to show xq eth
  25-Nov-03  DTH  Reworked interrupts to fix broken XQB implementation
  19-Nov-03  MP   Rearranged timer reset sequencing to allow for a device to be
                  disabled after it had been enabled.
  17-Nov-03  DTH  Standardized #include of timeb.h
  28-Sep-03  MP   - Fixed bug in xq_process_setup which would leave the
                  device in promiscuous or all multicast mode once it
                  ever had been there.
                  - Fixed output format in show_xq_sanity to end in "\n"
                  - Added display of All Multicast and promiscuous to
                  xq_show_filters
                  - The stuck in All Multicast or Promiscuous issue is 
                  worse than previously thought.  See comments in 
                  xq_process_setup.                  
                  - Change xq_setmac to also allow ":" as a address 
                  separator character, since sim_ether's eth_mac_fmt
                  formats them with this separator character.
                  - Changed xq_sw_reset to behave more like the set of
                  actions described in Table 3-6 of the DELQA manual.
                  The manual mentions "N/A" which I'm interpreting to
                  mean "Not Affected".
  05-Jun-03  DTH  Added receive packet splitting
  03-Jun-03  DTH  Added SHOW XQ FILTERS
  02-Jun-03  DTH  Added SET/SHOW XQ STATS (packet statistics), runt & giant processing
  28-May-03  DTH  Modified message queue for dynamic size to shrink executable
  28-May-03  MP   Fixed bug in xq_setmac
  06-May-03  DTH  Changed 32-bit t_addr to uint32 for v3.0
                  Removed SET ADDRESS functionality
  05-May-03  DTH  Added second controller
  26-Mar-03  DTH  Added PDP11 bootrom loader
                  Adjusted xq_ex and xq_dev to allow pdp11 to look at bootrom
                  Patched bootrom to allow "pass" of diagnostics on RSTS/E
  06-Mar-03  DTH  Corrected interrupts on IE state transition (code by Tom Evans)
                  Added interrupt clear on soft reset (first noted by Bob Supnik)
                  Removed interrupt when setting XL or RL (multiple people)
  16-Jan-03  DTH  Merged Mark Pizzolato's enhancements with main source
                  Corrected PDP11 XQ_DEBUG compilation
  15-Jan-03  MP   Fixed the number of units in the xq device structure.
  13-Jan-03  MP   Reworked the timer management logic which initiated
                  the system id broadcast messages.  The original
                  implementation triggered this on the CSR transition
                  of Receiver Enabled.  This was an issue since the
                  it seems that at least VMS's XQ driver makes this
                  transition often and the resulting overhead reduces
                  the simulated CPU instruction execution throughput by
                  about 40%.  I start the system id timer on device
                  reset and it fires once a second so that it can
                  leverage the reasonably recalibrated tmr_poll value.
  13-Jan-03  MP   Changed the scheduling of xq_svc to leverage the
                  dynamically computed clock values to achieve an
                  approximate interval of 100 per second.  This is
                  more than sufficient for normal system behaviour
                  expecially since we service receives with every
                  transmit.  The previous fixed value of 2500
                  attempted to get 200/sec but it was a guess that
                  didn't adapt.  On faster host systems (possibly
                  most of them) the 2500 number spends too much time
                  polling.
  10-Jan-03  DTH  Removed XQ_DEBUG dependency from Borland #pragmas
                  Added SET XQ BOOTROM command for PDP11s
  07-Jan-03  DTH  Added pointer to online manuals
  02-Jan-03  DTH  Added local packet processing
  30-Dec-02  DTH  Added automatic system id broadcast
  27-Dec-02  DTH  Merged Mark Pizzolato's enhancements with main source
  20-Dec-02  MP   Fix bug that caused VMS system crashes when attempting cluster
                  operations.  Added additional conditionally compiled debug
                  info needed to track down the issue.
  17-Dec-02  MP   Added SIMH "registers" describing the Ethernet state
                  so this information can be recorded in a "saved" snapshot.
  05-Dec-02  MP   Adjusted the rtime value from 100 to 2500 which increased the
                  available CPU cycles for Instruction execution by almost 100%.
                  This made sense after the below enhancements which, in general
                  caused the draining of the received data stream much more
                  agressively with less overhead.
  05-Dec-02  MP   Added a call to xq_svc after all successful calls to eth_write
                  to allow receive processing to happen before the next event
                  service time.
  05-Dec-02  MP   Restructured the flow of processing in xq_svc so that eth_read
                  is called repeatedly until either a packet isn't found or
                  there is no room for another one in the queue.  Once that has
                  been done, xq_process_rdbl is called to pass the queued packets
                  into the simulated system as space is available there.
                  xq_process_rdbl is also called at the beginning of xq_svc to
                  drain the queue into the simulated system, making more room
                  available in the queue.  No processing is done at all in
                  xq_svc if the receiver is disabled.
  04-Dec-02  MP   Changed interface and usage to xq_insert_queue to pass
                  the packet to be inserted by reference.  This avoids 3K bytes
                  of buffer copy operations for each packet received.  Now only
                  copy actual received packet data.
  31-Oct-02  DTH  Cleaned up pointer warnings (found by Federico Schwindt)
                  Corrected unattached and no network behavior
                  Added message when SHOW XQ ETH finds no devices
  23-Oct-02  DTH  Beta 5 released
  22-Oct-02  DTH  Added all_multicast and promiscuous support
  21-Oct-02  DTH  Added write buffer max size check (code by Jason Thorpe)
                  Corrected copyright again
                  Implemented NXM testing and recovery
  16-Oct-02  DTH  Beta 4 released
                  Added and debugged Sanity Timer code
                  Corrected copyright
  15-Oct-02  DTH  Rollback to known good Beta3 and roll forward; TCP broken
  12-Oct-02  DTH  Fixed VAX network bootstrap; setup packets must return TDR > 0
  11-Oct-02  DTH  Added SET/SHOW XQ TYPE and SET/SHOW XQ SANITY commands
  10-Oct-02  DTH  Beta 3 released; Integrated with 2.10-0b1
                  Fixed off-by-1 bug on xq->setup.macs[7..13]
                  Added xq_make_checksum
                  Added rejection of multicast addresses in SET XQ MAC
  08-Oct-02  DTH  Beta 2 released; Integrated with 2.10-0p4
                  Added variable vector (fixes PDP11) and copyrights
  03-Oct-02  DTH  Beta version of xq/sim_ether released for SIMH 2.09-11
  24-Sep-02  DTH  Moved more code to Sim_Ether module, added SHOW ETH command
  23-Sep-02  DTH  Added SET/SHOW MAC command
  22-Sep-02  DTH  Multinet TCP/IP loaded, tests OK via SET HOST/TELNET
  20-Sep-02  DTH  Cleaned up code fragments, fixed non-DECNET MAC use
  19-Sep-02  DTH  DECNET finally stays up; successful SET HOST to another node
  15-Sep-02  DTH  Added ethernet packet read/write
  13-Sep-02  DTH  DECNET starts, but circuit keeps going up & down
  26-Aug-02  DTH  DECNET loaded, returns device timeout
  22-Aug-02  DTH  VMS 7.2 recognizes device as XQA0
  18-Aug-02  DTH  VAX sees device as XQA0; shows hardcoded MAC correctly
  15-Aug-02  DTH  Started XQ simulation

  ------------------------------------------------------------------------------
*/

#include <assert.h>
#include "pdp11_xq.h"
#include "pdp11_xq_bootrom.h"

/*
 * Multiprocessor note:
 *
 *   VAX MP supports only DEQNA and DELQA since VSMP currently provides XQDRIVER patches
 *   only for these two controllers. DELQA PLUS is not supported: VSMP patches for it 
 *   are not provided, and code in xq_process_turbo_rbdl and xq_process_turbo_xbdl also
 *   had not been modified to provide appropriate memory barriers.
 *
 ******************************************************************
 *
 * Note: Current code in InterlockedOpLock::virt_lock and InterlockedOpLock::phys_lock
 *       assumes that XQ performs access to BDL only within the context of VCPU threads, 
 *       not in IOP thread. If it were to change, phys_lock and virt_lock should be modified
 *       to issue smp_mb even in uniprocessor case, since XQDRIVER patches use BBSSI
 *       to issue memory barrier and since XQDRIVER code itself also uses interlocked
 *       instructions to issue memory barrier. The code that implements VAXMP_API_OP_MEMBAR
 *       would need to be also modified to always issue memory barrier, even in uniprocessor
 *       case.
 */

/* forward declarations */
t_stat xq_rd(int32* data, int32 PA, int32 access);
t_stat xq_wr(int32  data, int32 PA, int32 access);
t_stat xq_svc(RUN_SVC_DECL, UNIT * uptr);
t_stat xq_svc_ex(RUN_DECL, UNIT * uptr, CTLR* xq);
t_stat xq_tmrsvc(RUN_SVC_DECL, UNIT * uptr);
t_stat xq_reset (DEVICE * dptr);
t_stat xq_attach (UNIT * uptr, char * cptr);
t_stat xq_detach (UNIT * uptr);
t_stat xq_showmac (SMP_FILE* st, UNIT* uptr, int32 val, void* desc);
t_stat xq_setmac  (UNIT* uptr, int32 val, char* cptr, void* desc);
t_stat xq_show_filters (SMP_FILE* st, UNIT* uptr, int32 val, void* desc);
t_stat xq_show_stats (SMP_FILE* st, UNIT* uptr, int32 val, void* desc);
t_stat xq_set_stats  (UNIT* uptr, int32 val, char* cptr, void* desc);
t_stat xq_show_type (SMP_FILE* st, UNIT* uptr, int32 val, void* desc);
t_stat xq_set_type (UNIT* uptr, int32 val, char* cptr, void* desc);
t_stat xq_show_sanity (SMP_FILE* st, UNIT* uptr, int32 val, void* desc);
t_stat xq_set_sanity (UNIT* uptr, int32 val, char* cptr, void* desc);
t_stat xq_show_poll (SMP_FILE* st, UNIT* uptr, int32 val, void* desc);
t_stat xq_set_poll (UNIT* uptr, int32 val, char* cptr, void* desc);
t_stat xq_process_xbdl(CTLR* xq);
t_stat xq_dispatch_xbdl(CTLR* xq);
t_stat xq_process_turbo_rbdl(CTLR* xq);
t_stat xq_process_turbo_xbdl(CTLR* xq);
void xq_start_receiver(CTLR* xq);
void xq_stop_receiver(CTLR* xq);
void xq_sw_reset(CTLR* xq);
t_stat xq_ex (t_value *vptr, t_addr addr, UNIT *uptr, int32 sw);
t_stat xq_dep (t_value val, t_addr addr, UNIT *uptr, int32 sw);
void xq_reset_santmr(CTLR* xq);
t_stat xq_boot_host(CTLR* xq);
t_stat xq_system_id(CTLR* xq, const ETH_MAC dst, uint16 receipt_id);
void xqa_read_callback(int status);
void xqb_read_callback(int status);
void xqa_write_callback(int status);
void xqb_write_callback(int status);
void xq_setint (CTLR* xq);
void xq_clrint (CTLR* xq, t_bool intack = FALSE);
int32 xq_int (void);
void xq_csr_set_clr(CTLR* xq, uint16 set_bits, uint16 clear_bits);
static int32 xq_update_bdl_status_words(RUN_DECL, uint32 bdl_ba, uint16* stw);
static int32 xq_fetch_bdl_entry(RUN_DECL, uint32 bdl_ba, uint16* buf);
static void xq_activate(UNIT* uptr, t_bool try_at_idletime, uint32 fraction);
static void xq_activate_abs(UNIT* uptr, t_bool try_at_idletime, uint32 fraction);

struct xq_device    xqa = {
  xqa_read_callback,                        /* read callback routine */
  xqa_write_callback,                       /* write callback routine */
  {0x08, 0x00, 0x2B, 0xAA, 0xBB, 0xCC},     /* mac */
#if defined(VM_VAX_MP)
  XQ_T_DELQA,                               /* type */
#else
  XQ_T_DELQA_PLUS,                          /* type */
#endif
  XQ_T_DELQA,                               /* mode */
  XQ_SERVICE_INTERVAL,                      /* poll */
  0, 0,                                     /* coalesce */
  {0}                                       /* sanity */
  };

struct xq_device    xqb = {
  xqb_read_callback,                        /* read callback routine */
  xqb_write_callback,                       /* write callback routine */
  {0x08, 0x00, 0x2B, 0xBB, 0xCC, 0xDD},     /* mac */
#if defined(VM_VAX_MP)
  XQ_T_DELQA,                               /* type */
#else
  XQ_T_DELQA_PLUS,                          /* type */
#endif
  XQ_T_DELQA,                               /* mode */
  XQ_SERVICE_INTERVAL,                      /* poll */
  0, 0,                                     /* coalesce */
  {0}                                       /* sanity */
  };

/* SIMH device structures */
DIB xqa_dib = { IOBA_XQ, IOLN_XQ, &xq_rd, &xq_wr,
        1, IVCL (XQ), 0, { &xq_int } };

UNIT* xqa_unit[] = {
   UDATA (&xq_svc, UNIT_IDLE|UNIT_ATTABLE|UNIT_DISABLE, 2047),   /* receive timer */
   UDATA (&xq_tmrsvc, UNIT_IDLE|UNIT_DIS, 0)
};

REG xqa_reg[] = {
  { GRDATA_GBL ( SA0,  xqa.mac[0], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA_GBL ( SA1,  xqa.mac[1], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA_GBL ( SA2,  xqa.mac[2], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA_GBL ( SA3,  xqa.mac[3], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA_GBL ( SA4,  xqa.mac[4], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA_GBL ( SA5,  xqa.mac[5], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA_GBL ( MX0,  xqa.mac_checksum[0], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA_GBL ( MX1,  xqa.mac_checksum[1], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA_GBL ( RBDL, xqa.rbdl[0], XQ_RDX, 16, 0), REG_FIT },
  { GRDATA_GBL ( RBDH, xqa.rbdl[1], XQ_RDX, 16, 0), REG_FIT },
  { GRDATA_GBL ( XBDL, xqa.xbdl[0], XQ_RDX, 16, 0), REG_FIT },
  { GRDATA_GBL ( XBDH, xqa.xbdl[1], XQ_RDX, 16, 0), REG_FIT },
  { GRDATA_GBL ( VAR,  xqa.var,  XQ_RDX, 16, 0), REG_FIT },
  { GRDATA_GBL ( CSR,  xqa.csr,  XQ_RDX, 16, 0), REG_FIT },
  { FLDATA_GBL ( INT,  xqa.irq, 0) },
  { GRDATA_GBL ( TYPE,  xqa.type,  XQ_RDX, 32, 0), REG_FIT },
  { GRDATA_GBL ( MODE,  xqa.mode,  XQ_RDX, 32, 0), REG_FIT },
  { GRDATA_GBL ( POLL, xqa.poll, XQ_RDX, 16, 0), REG_HRO},
  { GRDATA_GBL ( CLAT, xqa.coalesce_latency, XQ_RDX, 16, 0), REG_HRO},
  { GRDATA_GBL ( CLATT, xqa.coalesce_latency_ticks, XQ_RDX, 16, 0), REG_HRO},
  { GRDATA_GBL ( RBDL_BA, xqa.rbdl_ba, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( XBDL_BA, xqa.xbdl_ba, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( SETUP_PRM, xqa.setup.promiscuous, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( SETUP_MLT, xqa.setup.multicast, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( SETUP_L1, xqa.setup.l1, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( SETUP_L2, xqa.setup.l2, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( SETUP_L3, xqa.setup.l3, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( SETUP_SAN, xqa.setup.sanity_timer, XQ_RDX, 32, 0), REG_HRO},
  { BRDATA_GBL ( SETUP_MACS, &xqa.setup.macs, XQ_RDX, 8, sizeof(xqa.setup.macs)), REG_HRO},
  { BRDATA_GBL ( STATS, &xqa.stats, XQ_RDX, 8, sizeof(xqa.setup.macs)), REG_HRO},
  { BRDATA_GBL ( TURBO_INIT, &xqa.init, XQ_RDX, 8, sizeof(xqa.init)), REG_HRO},
  { GRDATA_GBL ( SRR,  xqa.srr,  XQ_RDX, 16, 0), REG_FIT },
  { GRDATA_GBL ( SRQR,  xqa.srqr,  XQ_RDX, 16, 0), REG_FIT },
  { GRDATA_GBL ( IBA,  xqa.iba,  XQ_RDX, 32, 0), REG_FIT },
  { GRDATA_GBL ( ICR,  xqa.icr,  XQ_RDX, 16, 0), REG_FIT },
  { GRDATA_GBL ( IPEND,  xqa.pending_interrupt,  XQ_RDX, 16, 0), REG_FIT },
  { GRDATA_GBL ( TBINDX, xqa.tbindx, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( RBINDX, xqa.rbindx, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( IDTMR, xqa.idtmr, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( VECTOR, xqa_dib.vec, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( MUST_POLL, xqa.must_poll, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( SANT_ENAB, xqa.sanity.enabled, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( SANT_QSECS, xqa.sanity.quarter_secs, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( SANT_TIMR, xqa.sanity.timer, XQ_RDX, 32, 0), REG_HRO},
  { NULL },
};

DIB xqb_dib = { IOBA_XQB, IOLN_XQB, &xq_rd, &xq_wr,
        1, IVCL (XQ), 0, { &xq_int } };

UNIT* xqb_unit[] = {
   UDATA (&xq_svc, UNIT_IDLE|UNIT_ATTABLE|UNIT_DISABLE, 2047),  /* receive timer */
   UDATA (&xq_tmrsvc, UNIT_IDLE|UNIT_DIS, 0)
};

REG xqb_reg[] = {
  { GRDATA_GBL ( SA0,  xqb.mac[0], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA_GBL ( SA1,  xqb.mac[1], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA_GBL ( SA2,  xqb.mac[2], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA_GBL ( SA3,  xqb.mac[3], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA_GBL ( SA4,  xqb.mac[4], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA_GBL ( SA5,  xqb.mac[5], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA_GBL ( MX0,  xqb.mac_checksum[0], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA_GBL ( MX1,  xqb.mac_checksum[1], XQ_RDX, 8, 0), REG_RO|REG_FIT},
  { GRDATA_GBL ( RBDL, xqb.rbdl[0], XQ_RDX, 16, 0), REG_FIT },
  { GRDATA_GBL ( RBDH, xqb.rbdl[1], XQ_RDX, 16, 0), REG_FIT },
  { GRDATA_GBL ( XBDL, xqb.xbdl[0], XQ_RDX, 16, 0), REG_FIT },
  { GRDATA_GBL ( XBDH, xqb.xbdl[1], XQ_RDX, 16, 0), REG_FIT },
  { GRDATA_GBL ( VAR,  xqb.var,  XQ_RDX, 16, 0), REG_FIT },
  { GRDATA_GBL ( CSR,  xqb.csr,  XQ_RDX, 16, 0), REG_FIT },
  { FLDATA_GBL ( INT,  xqb.irq, 0) },
  { GRDATA_GBL ( TYPE,  xqb.type,  XQ_RDX, 32, 0), REG_FIT },
  { GRDATA_GBL ( MODE,  xqb.mode,  XQ_RDX, 32, 0), REG_FIT },
  { GRDATA_GBL ( POLL, xqb.poll, XQ_RDX, 16, 0), REG_HRO},
  { GRDATA_GBL ( CLAT, xqb.coalesce_latency, XQ_RDX, 16, 0), REG_HRO},
  { GRDATA_GBL ( CLATT, xqb.coalesce_latency_ticks, XQ_RDX, 16, 0), REG_HRO},
  { GRDATA_GBL ( RBDL_BA, xqb.rbdl_ba, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( XBDL_BA, xqb.xbdl_ba, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( SETUP_PRM, xqb.setup.promiscuous, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( SETUP_MLT, xqb.setup.multicast, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( SETUP_L1, xqb.setup.l1, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( SETUP_L2, xqb.setup.l2, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( SETUP_L3, xqb.setup.l3, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( SETUP_SAN, xqb.setup.sanity_timer, XQ_RDX, 32, 0), REG_HRO},
  { BRDATA_GBL ( SETUP_MACS, &xqb.setup.macs, XQ_RDX, 8, sizeof(xqb.setup.macs)), REG_HRO},
  { BRDATA_GBL ( STATS, &xqb.stats, XQ_RDX, 8, sizeof(xqa.setup.macs)), REG_HRO},
  { BRDATA_GBL ( TURBO_INIT, &xqb.init, XQ_RDX, 8, sizeof(xqb.init)), REG_HRO},
  { GRDATA_GBL ( SRR,  xqb.srr,  XQ_RDX, 16, 0), REG_FIT },
  { GRDATA_GBL ( SRQR,  xqb.srqr,  XQ_RDX, 16, 0), REG_FIT },
  { GRDATA_GBL ( IBA,  xqb.iba,  XQ_RDX, 32, 0), REG_FIT },
  { GRDATA_GBL ( ICR,  xqb.icr,  XQ_RDX, 16, 0), REG_FIT },
  { GRDATA_GBL ( IPEND,  xqb.pending_interrupt,  XQ_RDX, 16, 0), REG_FIT },
  { GRDATA_GBL ( TBINDX, xqb.tbindx, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( RBINDX, xqb.rbindx, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( IDTMR, xqb.idtmr, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( VECTOR, xqb_dib.vec, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( MUST_POLL, xqb.must_poll, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( SANT_ENAB, xqb.sanity.enabled, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( SANT_QSECS, xqb.sanity.quarter_secs, XQ_RDX, 32, 0), REG_HRO},
  { GRDATA_GBL ( SANT_TIMR, xqb.sanity.timer, XQ_RDX, 32, 0), REG_HRO},
  { NULL },
};

MTAB xq_mod[] = {
  { MTAB_XTD|MTAB_VDV, 004, "ADDRESS", NULL,
    NULL, &show_addr, NULL },
  { MTAB_XTD|MTAB_VDV, 0, "VECTOR", NULL,
    NULL, &show_vec, NULL },
  { MTAB_XTD | MTAB_VDV, 0, "MAC", "MAC=xx:xx:xx:xx:xx:xx",
    &xq_setmac, &xq_showmac, NULL },
  { MTAB_XTD | MTAB_VDV | MTAB_NMO, 0, "ETH", "ETH",
    NULL, &eth_show, NULL },
  { MTAB_XTD | MTAB_VDV | MTAB_NMO, 0, "FILTERS", "FILTERS",
    NULL, &xq_show_filters, NULL },
  { MTAB_XTD | MTAB_VDV | MTAB_NMO, 0, "STATS", "STATS",
    &xq_set_stats, &xq_show_stats, NULL },
  { MTAB_XTD | MTAB_VDV, 0, "TYPE", "TYPE={DEQNA|DELQA|DELQA-T}",
    &xq_set_type, &xq_show_type, NULL },
#ifdef USE_READER_THREAD
  { MTAB_XTD | MTAB_VDV, 0, "POLL", "POLL={DEFAULT|DISABLED|4..2500|DELAY=nnn}",
    &xq_set_poll, &xq_show_poll, NULL },
#else
  { MTAB_XTD | MTAB_VDV, 0, "POLL", "POLL={DEFAULT|DISABLED|4..2500}",
    &xq_set_poll, &xq_show_poll, NULL },
#endif
  { MTAB_XTD | MTAB_VDV | MTAB_NMO, 0, "SANITY", "SANITY={ON|OFF}",
    &xq_set_sanity, &xq_show_sanity, NULL },
  { 0 },
};

DEBTAB xq_debug[] = {
  {"TRACE",  DBG_TRC},
  {"CSR",    DBG_CSR},
  {"VAR",    DBG_VAR},
  {"WARN",   DBG_WRN},
  {"SETUP",  DBG_SET},
  {"SANITY", DBG_SAN},
  {"REG",    DBG_REG},
  {"PACKET", DBG_PCK},
  {"DATA",   DBG_DAT},
  {"ETH",    DBG_ETH},
  {0}
};

DEVICE xq_dev = {
  "XQ", xqa_unit, xqa_reg, xq_mod,
  2, XQ_RDX, 11, 1, XQ_RDX, 16,
  &xq_ex, &xq_dep, &xq_reset,
  NULL, &xq_attach, &xq_detach,
  &xqa_dib, /*DEV_FLTA |*/ DEV_DISABLE | DEV_QBUS | DEV_DEBUG,
  0, xq_debug
};

DEVICE xqb_dev = {
  "XQB", xqb_unit, xqb_reg, xq_mod,
  2, XQ_RDX, 11, 1, XQ_RDX, 16,
  &xq_ex, &xq_dep, &xq_reset,
  NULL, &xq_attach, &xq_detach,
  &xqb_dib, DEV_FLTA | DEV_DISABLE | DEV_DIS | DEV_QBUS | DEV_DEBUG,
  0, xq_debug
};

AUTO_INIT_DEVLOCK(xqa_lock);
AUTO_INIT_DEVLOCK(xqb_lock);
static smp_interlocked_uint32_var xq_pending_intrs = smp_var_init(0);    /* active interrupt count */

CTLR xq_ctrl[] = {
  {&xq_dev,  xqa_unit, &xqa_dib, &xqa, & xqa_lock},       /* XQA controller */
  {&xqb_dev, xqb_unit, &xqb_dib, &xqb, & xqb_lock}        /* XQB controller */
};

const char* const xq_recv_regnames[] = {
  "MAC0", "MAC1", "MAC2", "MAC3", "MAC4", "MAC5", "VAR", "CSR"
};

const char* const xqt_recv_regnames[] = {
  "MAC0", "MAC1", "MAC2", "MAC3", "MAC4", "MAC5", "SRR", ""
};

const char* const xq_xmit_regnames[] = {
  "XCR0", "XCR1", "RBDL-Lo", "RBDL-Hi", "XBDL-Lo", "XBDL-Hi", "VAR", "CSR"
};

const char* const xqt_xmit_regnames[] = {
  "IBAL", "IBAH", "ICR", "", "SRQR", "", "", "ARQR"
};

const char* const xq_csr_bits[] = {
  "RE", "SR", "NI", "BD", "XL", "RL", "IE", "XI",
  "IL", "EL", "SE", "RR", "OK", "CA", "PE", "RI"
};

const char* const xq_var_bits[] = {
  "ID", "RR", "V0", "V1", "V2", "V3", "V4", "V5",
  "V6", "V7", "S1", "S2", "S3", "RS", "OS", "MS"
};

const char* const xq_srr_bits[] = {
  "RS0", "RS1", "",    "",    "",    "",    "",    "",
  "",    "TBL", "IME", "PAR", "NXM", "",    "CHN", "FES"
};

/* internal debugging routines */
void xq_debug_setup(CTLR* xq);
void xq_debug_turbo_setup(CTLR* xq);

static void init_xq_data()
{
    smp_check_aligned(& xq_pending_intrs);
}
ON_INIT_INVOKE(init_xq_data);

/*============================================================================*/

/* Multicontroller support */

CTLR* xq_unit2ctlr(UNIT* uptr)
{
  unsigned int i,j;
  for (i=0; i<XQ_MAX_CONTROLLERS; i++)
    for (j=0; j<xq_ctrl[i].dev->numunits; j++)
      if (xq_ctrl[i].unit[j] == uptr)
        return &xq_ctrl[i];
  /* not found */
  return 0;
}

CTLR* xq_dev2ctlr(DEVICE* dptr)
{
  int i;
  for (i=0; i<XQ_MAX_CONTROLLERS; i++)
    if (xq_ctrl[i].dev == dptr)
      return &xq_ctrl[i];
  /* not found */
  return 0;
}

CTLR* xq_pa2ctlr(uint32 PA)
{
  int i;
  for (i=0; i<XQ_MAX_CONTROLLERS; i++)
    if ((PA >= xq_ctrl[i].dib->ba) && (PA < (xq_ctrl[i].dib->ba + xq_ctrl[i].dib->lnt)))
      return &xq_ctrl[i];
  /* not found */
  return 0;
}

/*============================================================================*/

/* stop simh from reading non-existant unit data stream */
t_stat xq_ex (t_value* vptr, t_addr addr, UNIT* uptr, int32 sw)
{
  /* on PDP-11, allow EX command to look at bootrom */
#ifdef VM_PDP11
  if (addr <= sizeof(xq_bootrom)/2)
    *vptr = xq_bootrom[addr];
  else
    *vptr = 0;
  return SCPE_OK;
#else
  return SCPE_NOFNC;
#endif
}

/* stop simh from writing non-existant unit data stream */
t_stat xq_dep (t_value val, t_addr addr, UNIT* uptr, int32 sw)
{
  return SCPE_NOFNC;
}

t_stat xq_showmac (SMP_FILE* st, UNIT* uptr, int32 val, void* desc)
{
  CTLR* xq = xq_unit2ctlr(uptr);
  char  buffer[20];

  eth_mac_fmt((ETH_MAC*)xq->var->mac, buffer);
  fprintf(st, "MAC=%s", buffer);
  return SCPE_OK;
}

void xq_make_checksum(CTLR* xq)
{
  /* checksum calculation routine detailed in vaxboot.zip/xqbtdrivr.mar */
  uint32  checksum = 0;
  const uint32 wmask = 0xFFFF;
  int i;

  for (i = 0; i < (int) sizeof(ETH_MAC); i += 2) {
    checksum <<= 1;
    if (checksum > wmask)
      checksum -= wmask;
    checksum += (xq->var->mac[i] << 8) | xq->var->mac[i+1];
    if (checksum > wmask)
      checksum -= wmask;
  }
  if (checksum == wmask)
    checksum = 0;

  /* set checksum bytes */
  xq->var->mac_checksum[0] = checksum & 0xFF;
  xq->var->mac_checksum[1] = checksum >> 8;
}

t_stat xq_setmac (UNIT* uptr, int32 val, char* cptr, void* desc)
{
  t_stat status;
  CTLR* xq = xq_unit2ctlr(uptr);

  if (!cptr) return SCPE_IERR;
  if (uptr->flags & UNIT_ATT) return SCPE_ALATT;
  status = eth_mac_scan(&xq->var->mac, cptr);
  if (status != SCPE_OK)
    return status;

  /* calculate mac checksum */
  xq_make_checksum(xq);
  return SCPE_OK;
}

t_stat xq_set_stats (UNIT* uptr, int32 val, char* cptr, void* desc)
{
  /* this sets all ints in the stats structure to the integer passed */
  CTLR* xq = xq_unit2ctlr(uptr);

  if (cptr) {
    /* set individual stats to passed parameter value */
    int init = atoi(cptr);
    int* stat_array = (int*) &xq->var->stats;
    int elements = sizeof(struct xq_stats)/sizeof(int);
    int i;
    for (i=0; i<elements; i++)
      stat_array[i] = init;
  } else {
    /* set stats to zero */
    memset(&xq->var->stats, 0, sizeof(struct xq_stats));
  }
  return SCPE_OK;
}

t_stat xq_show_stats (SMP_FILE* st, UNIT* uptr, int32 val, void* desc)
{
  char* fmt = "  %-15s%d\n";
  CTLR* xq = xq_unit2ctlr(uptr);

  fprintf(st, "XQ Ethernet statistics:\n");
  fprintf(st, fmt, "Recv:",        xq->var->stats.recv);
  fprintf(st, fmt, "Dropped:",     xq->var->stats.dropped + xq->var->ReadQ.loss);
  fprintf(st, fmt, "Xmit:",        xq->var->stats.xmit);
  fprintf(st, fmt, "Xmit Fail:",   xq->var->stats.fail);
  fprintf(st, fmt, "Runts:",       xq->var->stats.runt);
  fprintf(st, fmt, "Oversize:",    xq->var->stats.giant);
  fprintf(st, fmt, "SW Reset:",    xq->var->stats.reset);
  fprintf(st, fmt, "Setup:",       xq->var->stats.setup);
  fprintf(st, fmt, "Loopback:",    xq->var->stats.loop);
  fprintf(st, fmt, "ReadQ count:", xq->var->ReadQ.count);
  fprintf(st, fmt, "ReadQ high:",  xq->var->ReadQ.high);
  eth_show_dev(st, xq->var->etherface);
  return SCPE_OK;
}

t_stat xq_show_filters (SMP_FILE* st, UNIT* uptr, int32 val, void* desc)
{
  CTLR* xq = xq_unit2ctlr(uptr);
  char  buffer[20];
  int i;

  if (xq->var->mode == XQ_T_DELQA_PLUS) {
    eth_mac_fmt(&xq->var->init.phys, buffer);
    fprintf(st, "Physical Address=%s\n", buffer);
    if (xq->var->etherface->hash_filter) {
      fprintf(st, "Multicast Hash: ");
      for (i=0; i < (int) sizeof(xq->var->etherface->hash); ++i)
        fprintf(st, "%02X ", xq->var->etherface->hash[i]);
      fprintf(st, "\n");
    }
    if (xq->var->init.mode & XQ_IN_MO_PRO)
      fprintf(st, "Promiscuous Receive Mode\n");
  } else {
    fprintf(st, "Filters:\n");
    for (i=0; i<XQ_FILTER_MAX; i++) {
      eth_mac_fmt((ETH_MAC*)xq->var->setup.macs[i], buffer);
      fprintf(st, "  [%2d]: %s\n", i, buffer);
    }
    if (xq->var->setup.multicast)
      fprintf(st, "All Multicast Receive Mode\n");
    if (xq->var->setup.promiscuous)
      fprintf(st, "Promiscuous Receive Mode\n");
  }
  return SCPE_OK;
}

t_stat xq_show_type (SMP_FILE* st, UNIT* uptr, int32 val, void* desc)
{
  CTLR* xq = xq_unit2ctlr(uptr);
  fprintf(st, "type=");
  switch (xq->var->type) {
    case  XQ_T_DEQNA:       fprintf(st, "DEQNA");      break;
    case  XQ_T_DELQA:       fprintf(st, "DELQA");      break;
    case  XQ_T_DELQA_PLUS:  fprintf(st, "DELQA-T");    break;
  }
  if (xq->var->type != xq->var->mode) {
    fprintf(st, ",mode=");
    switch (xq->var->mode) {
      case  XQ_T_DEQNA:       fprintf(st, "DEQNA");      break;
      case  XQ_T_DELQA:       fprintf(st, "DELQA");      break;
      case  XQ_T_DELQA_PLUS:  fprintf(st, "DELQA-T");    break;
    }
  }
  return SCPE_OK;
}

t_stat xq_set_type (UNIT* uptr, int32 val, char* cptr, void* desc)
{
  CTLR* xq = xq_unit2ctlr(uptr);
  if (!cptr) return SCPE_IERR;
  if (uptr->flags & UNIT_ATT) return SCPE_ALATT;

  /* this assumes that the parameter has already been upcased */
  if      (!strcmp(cptr, "DEQNA"))      xq->var->type = XQ_T_DEQNA;
  else if (!strcmp(cptr, "DELQA"))      xq->var->type = XQ_T_DELQA;
  else if (!strcmp(cptr, "DELQA-T"))    xq->var->type = XQ_T_DELQA_PLUS;
  else return SCPE_ARG;
#if defined(VM_VAX_MP)
  if (xq->var->type == XQ_T_DELQA_PLUS)
  {
      smp_printf("Warning!!! DELQA-T is not supported by VAX MP in multiprocessor mode.\n");
      if (sim_log)
          fprintf(sim_log, "Warning!!! DELQA-T is not supported by VAX MP in multiprocessor mode.\n");
  }
#endif
  xq->var->mode = XQ_T_DELQA;
  if (xq->var->type == XQ_T_DEQNA)
    xq->var->mode = XQ_T_DEQNA;

  return SCPE_OK;
}

t_stat xq_show_poll (SMP_FILE* st, UNIT* uptr, int32 val, void* desc)
{
  CTLR* xq = xq_unit2ctlr(uptr);
  if (xq->var->poll)
    fprintf(st, "poll=%d", xq->var->poll);
  else {
    fprintf(st, "polling=disabled");
    if (xq->var->coalesce_latency)
      fprintf(st, ",latency=%d", xq->var->coalesce_latency);
  }
  return SCPE_OK;
}

t_stat xq_set_poll (UNIT* uptr, int32 val, char* cptr, void* desc)
{
  CTLR* xq = xq_unit2ctlr(uptr);
  if (!cptr) return SCPE_IERR;
  if (uptr->flags & UNIT_ATT) return SCPE_ALATT;

  /* this assumes that the parameter has already been upcased */
  if (!strcmp(cptr, "DEFAULT"))
    xq->var->poll = XQ_SERVICE_INTERVAL;
  else if ((!strcmp(cptr, "DISABLED")) || (!strncmp(cptr, "DELAY=", 6))) {
    xq->var->poll = 0;
    if (!strncmp(cptr, "DELAY=", 6)) {
      int delay = 0;
      if (1 != sscanf(cptr+6, "%d", &delay))
        return SCPE_ARG;
      xq->var->coalesce_latency = delay;
      xq->var->coalesce_latency_ticks = (atomic_var(tmr_poll) * clk_tps * xq->var->coalesce_latency) / 1000000;
      }
    }
  else {
    int newpoll = 0;
    if (1 != sscanf(cptr, "%d", &newpoll))
      return SCPE_ARG;
    if ((newpoll == 0) ||
        ((!sim_idle_enab) && (newpoll >= 4) && (newpoll <= 2500)))
      xq->var->poll = newpoll;
    else
      return SCPE_ARG;
  }

  return SCPE_OK;
}

t_stat xq_show_sanity (SMP_FILE* st, UNIT* uptr, int32 val, void* desc)
{
  CTLR* xq = xq_unit2ctlr(uptr);

  fprintf(st, "sanity=");
  switch (xq->var->sanity.enabled) {
    case 2:  fprintf(st, "ON\n");  break;
    default: fprintf(st, "OFF\n"); break;
  }
  return SCPE_OK;
}

t_stat xq_set_sanity (UNIT* uptr, int32 val, char* cptr, void* desc)
{
  CTLR* xq = xq_unit2ctlr(uptr);
  if (!cptr) return SCPE_IERR;
  if (uptr->flags & UNIT_ATT) return SCPE_ALATT;

  /* this assumes that the parameter has already been upcased */
  if      (!strcmp(cptr, "ON"))  xq->var->sanity.enabled = 2;
  else if (!strcmp(cptr, "OFF")) xq->var->sanity.enabled = 0;
  else return SCPE_ARG;

  return SCPE_OK;
}

/*============================================================================*/

t_stat xq_nxm_error(CTLR* xq)
{
  const uint16 set_bits = XQ_CSR_NI | XQ_CSR_XI | XQ_CSR_XL | XQ_CSR_RL;
  sim_debug(DBG_WRN, xq->dev, "Non Existent Memory Error!\n");

  if (xq->var->mode == XQ_T_DELQA_PLUS) {
    /* set NXM and associated bits in SRR */
    xq->var->srr |= (XQ_SRR_FES | XQ_SRR_NXM);
    xq_setint(xq);
  } else
    /* set NXM and associated bits in CSR */
    xq_csr_set_clr(xq, set_bits , 0);
  return SCPE_OK;
}

/*
** write callback
*/
void xq_write_callback (CTLR* xq, int status)
{
  RUN_SCOPE;
  int32 wstatus;
  const uint16 TDR = 100 + xq->var->write_buffer.len * 8; /* arbitrary value */
  uint16 write_success[2] = {0};
  uint16 write_failure[2] = {XQ_DSC_C};
  write_success[1] = TDR & 0x03FF; /* Does TDR get set on successful packets ?? */
  write_failure[1] = TDR & 0x03FF; /* TSW2<09:00> */
  
  xq->var->stats.xmit += 1;

  /*
   * Multiprocessor note:
   *
   * Abstractly speaking, we might have put smp_mb here to ensure that update
   * of status words is not reordered before reading transmit buffer on a multiprocessor
   * system. However multiple system calls performed after buffer reading (including
   * lock acquisition by xqa_write_callback and xqb_write_callback just before calling
   * xq_write_callback) already do provide this assurance, so exlicit MB here would have 
   * been redundant.
   *
   * Also, xq_update_bdl_status_words does perform memory barrier internally before
   * writing status word 1.
   *
   */

  /* update write status words */
  if (status == 0) { /* success */
    if (DBG_PCK & xq->dev->dctrl)
      eth_packet_trace_ex(xq->var->etherface, xq->var->write_buffer.msg, xq->var->write_buffer.len, "xq-write", DBG_DAT & xq->dev->dctrl, DBG_PCK);
    wstatus = xq_update_bdl_status_words(RUN_PASS, xq->var->xbdl_ba, write_success);
  } else { /* failure */
    sim_debug(DBG_WRN, xq->dev, "Packet Write Error!\n");
    xq->var->stats.fail += 1;
    wstatus = xq_update_bdl_status_words(RUN_PASS, xq->var->xbdl_ba, write_failure);
  }
  if (wstatus) {
    xq_nxm_error(xq);
    return;
  }

  /* update csr */
  xq_csr_set_clr(xq, XQ_CSR_XI, 0);

  /* reset sanity timer */
  xq_reset_santmr(xq);

  /* clear write buffer */
  xq->var->write_buffer.len = 0;

}

void xqa_write_callback (int status)
{
  CTLR* xq = &xq_ctrl[0];
  AUTO_LOCK_NM(xq_autolock, *xq->xq_lock);
  xq_write_callback(xq, status);
}

void xqb_write_callback (int status)
{
  CTLR* xq = &xq_ctrl[1];
  AUTO_LOCK_NM(xq_autolock, *xq->xq_lock);
  xq_write_callback(xq, status);
}


/*
 * Multiprocessor note:
 *
 * Standard VMS XQDRIVER relies on VAX strong memory model where CPU sees XQ writes in the order
 * XQ executed them, and vice versa. This does not hold true when simulated on the host machine
 * with weaker memory consistency model, unless explicit memory barriers are used by both VMS
 * XQDRIVER and SIMH XQ handler, and are paired up.
 *
 *********************************************************************************************
 *
 * Let's first consider the case of updates initiated by XQ and flowing to XQDRIVER.
 * These are signalled by XQ at the end of processing BDL entry by XQ, by XQ changing flags in
 * BD status word 1.
 *
 * XQDRIVER on it side contains code that directly checks buffer descriptors in the BDL
 * for their completion/availability, by first checking flags in status word 1 (at BDL desc + 8), 
 * then content of status word 2 (at BDL + 10) and any other data.
 *
 * This does not present a problem if XQ fully processes BDL, sends an interrupt and driver scans
 * BDL in response to this interrupt, as interrupt signalling/delivery sequence peforms required
 * memory barriers.
 *
 * However it does present a problem if XQDRIVER scans BDL in response to an interrupt raised
 * after partial BDL processing by XQ, after timeout or on new IO request.
 *
 * Solution is provided by (1) supplying memory barriers in XQ handler, and (2) modifying active
 * XQDRIVER code with VSMP tool to inject appropriate memory barriers into the driver's code.
 *
 * General approach is that "check ST1" in the driver code is replaced with sequence
 *     "read ST1, RMB, dispatch on flags fetched from ST1". 
 * In particular sequences
 *     "check ST1, check ST2"
 * are replaced with 
 *     "check ST1, RMB, check ST2". 
 * RMB is implemented with BBSSI instruction.
 *
 * On XQ handler side, general sequence is:
 *
 *     perform buffer access
 *     optionally MB
 *     set all necessary BD words except ST1
 *     MB or WMB
 *     set ST1
 *
 * Do note that for multiprocessor case we reverse here, in routine xq_update_bdl_status_words,
 * writing order for BDL status words compared to what real DEQNA/DELQA does. Instead of writing
 * ST1, then ST2, we reverse the order. We first write status word 2, perform WMB and then write
 * status word 1. This alleviates race condition for the driver described in XQDRIVER's routine NEXTMSG
 * and makes changes to the driver code more manageable and providing stable result on weak-memory-model 
 * host.
 *
 * For XQDRIVER source module that interacts with DEQNA and DELQA, refer to [PHV_LAN.SRC]DEQNA.MAR
 * or corresponding LIS file in OpenVMS source listings.
 *
 * Call graph for XQDRIVER routines:
 *
 *     (qio) -> IOREQ -> INIT_DEQNA
 *     (qio) -> IOREQ -> QNA_XMIT
 *     (qio) -> IOREQ -> SUB_SETUP_MODE -> QNA_XMIT
 *     (qio) -> IOREQ -> SUB_SETUP_MODE -> QNA_START_RECEIVE
 *     (TQE entry) -> CTRL_TIMER_EXP -> SYSID_TIMER_EXP -> QNA_XMIT -> SETUP_XMTDSC_UV2
 *     (TQE entry) -> CTRL_TIMER_EXP -> QNA_START_RECEIVE
 *     (interrupt, timeout, IOREQ for some requests) -> fork -> RCV_COMPLETE, XMT_COMPLETE
 *     RCV_COMPLETE -> NEXTMSG, QNA_XMIT, QNA_START_RECEIVE
 *     XMT_COMPLETE -> UNMAP_XMTBUF, QNA_XMIT
 *
 * Note that SETUP_XMTDSC_UV2 applies also to any processor (including KA650) other than MicroVAX I.
 *
 * Scanning of Rx BDL is handled in:
 *
 *     routine NEXTMSG (around label 220$) - fixed by patch XQRX1
 *     routine QNA_XMIT (below label 20$ located below XMIT.ALLVAX) - no need to fix this one,
 *                                                        as existing code already causes MBs
 *
 * Scanning of Tx BDL is handled in:
 *
 *     routine NEXTMSG (at the beginng) - fixed by patch XQTX1
 *                     (around label 111$) - fixed by patch XQTX2
 *                     (below label 2$) - fixed by patch XQTX3
 *                     (below label 8$) - fixed by patch XQTX4
 *
 *********************************************************************************************
 *
 * Now let's consider the case of BDL updates flowing the other direction: initiated by XQDRIVER
 * and noticed asynchronously by XQ.
 *
 * These are signalled by XQDRIVER to XQ by setting "Valid" bit in BDL entry.
 *
 * Therefore proper sequence on the driver side should be:
 *
 *     setup buffer and descriptor
 *     WMB
 *     set "Valid" bit
 *     optionally write CSR
 *
 * On XQ side, it should be: detect "Valid" bit, RMB, access buffer and descriptor.
 *
 * Finally, after completion of request, XQDRIVER clears "Valid" bit and should execute WMB
 * at this point (after clearing the bit, before starting to modify descriptor fields).
 *
 * Valid bit setting in Rx descriptor is handled in:
 *
 *     routine QNA_START_RECEIVE (below QNA_START_RECEIVE.COMMON) - fixed by patch XQRX3
 *
 * Valid bit clearing in Rx descriptor is handled in:
 *
 *     routine NEXTMSG (below label 220$) - fixed by patch XQRX4
 *     routine INIT_DEQNA (below label 20$) - fixed by patch XQRX2
 *
 * Valid bit setting in Tx descriptor is handled in:
 *
 *     routine QNA_XMIT (above label XMIT.UV1) - fixed by patch XQTX6
 *     routine QNA_XMIT (below label 20$ located below XMIT.ALLVAX) - fixed by patch XQTX10
 *
 * Valid bit clearing in Tx descriptor is handled in:
 *
 *     routine UNNAP_XMTBUF (below label 5050$, two times) - fixed by patches XQTX7, XQTX8
 *     routine INIT_DEQNA (below label 30$) - fixed by patch XQTX5
 *
 *     in addition, we put memory barrier for clearning SETUP bit in Tx descriptor
 *     in routine XMT_COMPLETE (below label 20$) - by patch XQTX9, this is almost surely
 *     unnecessary, but just to be on the safe side
 *
 */
static int32 xq_update_bdl_status_words(RUN_DECL, uint32 bdl_ba, uint16* stw)
{
    int32 wstatus;
#if defined(VM_VAX_MP)
    /* write status word 2 */
    wstatus = Map_WriteW(RUN_PASS, bdl_ba + 10, 2, stw + 1);
    if (wstatus)  return wstatus;

    /* perform full memory barrier (rather than just wmb) so we could also eliminate (comment out) 
       smp_mb during xq_process_xbdl, and so avoid the overhead of double barriers */
    smp_mb();

    /* write status word 1 */
    wstatus = Map_WriteW(RUN_PASS, bdl_ba + 8, 2, stw);
#else
    /* write status words 1 and 2 */
    wstatus = Map_WriteW(RUN_PASS, bdl_ba + 8, 4, stw);
#endif
    return wstatus;
}

/* retrieve BDL from memory */
static int32 xq_fetch_bdl_entry(RUN_DECL, uint32 bdl_ba, uint16* buf, int32 bcnt)
{
    int32 rwstatus;

    buf[0] = 0xFFFF;
    rwstatus = Map_WriteW(RUN_PASS, bdl_ba, 2, &buf[0]);
    if (rwstatus)  return rwstatus;
#if defined(VM_VAX_MP)
    rwstatus = Map_ReadW(RUN_PASS, bdl_ba + 2, 2, &buf[1]);
    if (rwstatus)  return rwstatus;
    /* perform RMB after reading descriptor flags word, including Valid bit */
    smp_rmb();
    if (bcnt > 2)
        rwstatus = Map_ReadW(RUN_PASS, bdl_ba + 4, bcnt - 2, &buf[2]);
#else
    rwstatus = Map_ReadW(RUN_PASS, bdl_ba + 2, bcnt, &buf[1]);
#endif
    return rwstatus;
}

/* read registers: */
t_stat xq_rd(int32* data, int32 PA, int32 access)
{
  CTLR* xq = xq_pa2ctlr(PA);
  AUTO_LOCK_NM(xq_autolock, *xq->xq_lock);
  int index = (PA >> 1) & 07;   /* word index */

  sim_debug(DBG_REG, xq->dev, "xq_rd(PA=0x%08X [%s], access=%d)\n", PA, ((xq->var->mode == XQ_T_DELQA_PLUS) ? xqt_recv_regnames[index] : xq_recv_regnames[index]), access);
  switch (index) {
    case 0:
    case 1:
      /* return checksum in external loopback mode */
      if (xq->var->csr & XQ_CSR_EL)
        *data = 0xFF00 | xq->var->mac_checksum[index];
      else
        *data = 0xFF00 | xq->var->mac[index];
      break;
    case 2:
    case 3:
    case 4:
    case 5:
      *data = 0xFF00 | xq->var->mac[index];
      break;
    case 6:
      if (xq->var->mode != XQ_T_DELQA_PLUS) {
        sim_debug_u16(DBG_VAR, xq->dev, xq_var_bits, xq->var->var, xq->var->var, 0);
        sim_debug    (DBG_VAR, xq->dev, ", vec = 0%o\n", (xq->var->var & XQ_VEC_IV));
        *data = xq->var->var;
      } else {
        sim_debug_u16(DBG_VAR, xq->dev, xq_srr_bits, xq->var->srr, xq->var->srr, 0);
        *data = xq->var->srr;
      }
      break;
    case 7:
      sim_debug_u16(DBG_CSR, xq->dev, xq_csr_bits, xq->var->csr, xq->var->csr, 1);
      *data = xq->var->csr;
      break;
  }
  return SCPE_OK;
}


/* dispatch ethernet read request
   procedure documented in sec. 3.2.2 */

t_stat xq_process_rbdl(CTLR* xq)
{
  RUN_SCOPE;
  int32 rstatus, wstatus, rwstatus;
  uint16 b_length, w_length, rbl;
  uint32 address;
  ETH_ITEM* item;
  uint8* rbuf;

  if (xq->var->mode == XQ_T_DELQA_PLUS)
    return xq_process_turbo_rbdl(xq);

  sim_debug(DBG_TRC, xq->dev, "xq_process_rdbl\n");

  /* process buffer descriptors */
  while(1) {

    /* get receive bdl from memory */
    rwstatus = xq_fetch_bdl_entry(RUN_PASS, xq->var->rbdl_ba, xq->var->rbdl_buf, 6);
    if (rwstatus) return xq_nxm_error(xq);

    /* invalid buffer? */
    if (~xq->var->rbdl_buf[1] & XQ_DSC_V) {
      xq_csr_set_clr(xq, XQ_CSR_RL, 0);
      return SCPE_OK;
    }

    /* explicit chain buffer? */
    if (xq->var->rbdl_buf[1] & XQ_DSC_C) {
      xq->var->rbdl_ba = ((xq->var->rbdl_buf[1] & 0x3F) << 16) | xq->var->rbdl_buf[2];
      continue;
    }

    /* stop processing if nothing in read queue */
    if (!xq->var->ReadQ.count) break;

    /* get status words */
    rstatus = Map_ReadW(RUN_PASS, xq->var->rbdl_ba + 8, 4, &xq->var->rbdl_buf[4]);
    if (rstatus) return xq_nxm_error(xq);

    /* get host memory address */
    address = ((xq->var->rbdl_buf[1] & 0x3F) << 16) | xq->var->rbdl_buf[2];

    /* decode buffer length - two's complement (in words) */
    w_length = ~xq->var->rbdl_buf[3] + 1;
    b_length = w_length * 2;
    if (xq->var->rbdl_buf[1] & XQ_DSC_H) b_length -= 1;
    if (xq->var->rbdl_buf[1] & XQ_DSC_L) b_length -= 1;

    item = &xq->var->ReadQ.item[xq->var->ReadQ.head];
    rbl = item->packet.len;
    rbuf = item->packet.msg;

    /* see if packet must be size-adjusted or is splitting */
    if (item->packet.used) {
      int used = item->packet.used;
      rbl -= used;
      rbuf = &item->packet.msg[used];
    } else {
      /* adjust runt packets */
      if (rbl < ETH_MIN_PACKET) {
        xq->var->stats.runt += 1;
        sim_debug(DBG_WRN, xq->dev, "Runt detected, size = %d\n", rbl);
        /* pad runts with zeros up to minimum size - this allows "legal" (size - 60)
           processing of those weird short ARP packets that seem to occur occasionally */
        memset(&item->packet.msg[rbl], 0, ETH_MIN_PACKET-rbl);
        rbl = ETH_MIN_PACKET;
      };

      /* adjust oversized packets */
      if (rbl > ETH_MAX_PACKET) {
        xq->var->stats.giant += 1;
        sim_debug(DBG_WRN, xq->dev, "Giant detected, size=%d\n", rbl);
        /* trim giants down to maximum size - no documentation on how to handle the data loss */
        item->packet.len = ETH_MAX_PACKET;
        rbl = ETH_MAX_PACKET;
      };
    };

    /* make sure entire packet fits in buffer - if not, will need to split into multiple buffers */
    if (rbl > b_length)
      rbl = b_length;
    item->packet.used += rbl;
    
    /* send data to host */
    wstatus = Map_WriteB(RUN_PASS, address, rbl, rbuf);
    if (wstatus) return xq_nxm_error(xq);

    /* set receive size into RBL - RBL<10:8> maps into Status1<10:8>,
       RBL<7:0> maps into Status2<7:0>, and Status2<15:8> (copy) */

    xq->var->rbdl_buf[4] = 0;
    switch (item->type) {
      case 0: /* setup packet */
        xq->var->stats.setup += 1;
        xq->var->rbdl_buf[4] = 0x2700; /* set esetup and RBL 10:8 */
        break;
      case 1: /* loopback packet */
        xq->var->stats.loop += 1;
        xq->var->rbdl_buf[4] = 0x2000;         /* loopback flag */
        xq->var->rbdl_buf[4] |= (rbl & 0x0700); /* high bits of rbl */
        break;
      case 2: /* normal packet */
        rbl -= 60;    /* keeps max packet size in 11 bits */
        xq->var->rbdl_buf[4] = (rbl & 0x0700); /* high bits of rbl */
        break;
    }
    if (item->packet.used < item->packet.len)
      xq->var->rbdl_buf[4] |= 0xC000;            /* not last segment */
    xq->var->rbdl_buf[5] = ((rbl & 0x00FF) << 8) | (rbl & 0x00FF);
    if (xq->var->ReadQ.loss) {
      sim_debug(DBG_WRN, xq->dev, "ReadQ overflow!\n");
      xq->var->rbdl_buf[4] |= 0x0001;   /* set overflow bit */
      xq->var->stats.dropped += xq->var->ReadQ.loss;
      xq->var->ReadQ.loss = 0;          /* reset loss counter */
    }

    /* 
     * Ensure update of RX BDL status words is not reordered before sending the data to user.
     * We do not perform it as driver should check status word 1 flags first, and we do perform
     * memory barrier before writing it in xq_update_bdl_status_words.
     */
    // smp_wmb();

    /* update read status words*/
    wstatus = xq_update_bdl_status_words(RUN_PASS, xq->var->rbdl_ba, &xq->var->rbdl_buf[4]);
    if (wstatus) return xq_nxm_error(xq);

    /* remove packet from queue */
    if (item->packet.used >= item->packet.len)
      ethq_remove(&xq->var->ReadQ);

    /* mark transmission complete */
    xq_csr_set_clr(xq, XQ_CSR_RI, 0);

    /* set to next bdl (implicit chain) */
    xq->var->rbdl_ba += 12;

 } /* while */

  return SCPE_OK;
}

t_stat xq_process_mop(CTLR* xq)
{
  RUN_SCOPE;
  uint32 address;
  uint16 size;
  int32 wstatus;
  struct xq_meb* meb = (struct xq_meb*) &xq->var->write_buffer.msg[0200];
  const struct xq_meb* limit = (struct xq_meb*) &xq->var->write_buffer.msg[0400];

  sim_debug(DBG_TRC, xq->dev, "xq_process_mop()\n");

  if (xq->var->type == XQ_T_DEQNA)  /* DEQNA's don't MOP */
    return SCPE_NOFNC;

  while ((meb->type != 0) && (meb < limit)) {
    address = (meb->add_hi << 16) || (meb->add_mi << 8) || meb->add_lo;
    size    = (meb->siz_hi << 8) || meb->siz_lo;

    /* MOP stuff here - NOT YET FULLY IMPLEMENTED */
    sim_debug (DBG_WRN, xq->dev, "Processing MEB type: %d\n", meb->type);
    switch (meb->type) {
      case 0:   /* MOP Termination */
        break;
      case 1:   /* MOP Read Ethernet Address */
        wstatus = Map_WriteB(RUN_PASS, address, sizeof(ETH_MAC), (uint8*) &xq->var->setup.macs[0]);
        if (wstatus) return xq_nxm_error(xq);
        break;
      case 2:   /* MOP Reset System ID */
        break;
      case 3:   /* MOP Read Last MOP Boot */
        break;
      case 4:   /* MOP Read Boot Password */
        break;
      case 5:   /* MOP Write Boot Password */
        break;
      case 6:   /* MOP Read System ID */
        break;
      case 7:   /* MOP Write System ID */
        break;
      case 8:   /* MOP Read Counters */
        break;
      case 9:   /* Mop Read/Clear Counters */
        break;
      case 10:  /* DELQA-PLUS Board ROM Version */
        if (xq->var->type == XQ_T_DELQA_PLUS) {
          uint16 Delqa_Plus_ROM_Version[3] = {2, 0, 0}; /* 2.0.0 */
          wstatus = Map_WriteB(RUN_PASS, address, sizeof(Delqa_Plus_ROM_Version), (uint8*) Delqa_Plus_ROM_Version);
          if (wstatus) return xq_nxm_error(xq);
        }
        break;
    } /* switch */

    /* process next meb */
    meb += sizeof(struct xq_meb);

  } /* while */
  return SCPE_OK;
}

t_stat xq_process_setup(CTLR* xq)
{
  int i,j;
  int count = 0;
  float secs;
  t_stat status;
  uint32 saved_debug = xq->dev->dctrl;
  ETH_MAC zeros = {0, 0, 0, 0, 0, 0};
  ETH_MAC filters[XQ_FILTER_MAX + 1];

  sim_debug(DBG_TRC, xq->dev, "xq_process_setup()\n");

  /* temporarily turn on Ethernet debugging if setup debugging is enabled */
  if (xq->dev->dctrl & DBG_SET)
    xq->dev->dctrl |= DBG_ETH;

  /* extract filter addresses from setup packet */
  memset(xq->var->setup.macs, '\0', sizeof(xq->var->setup.macs));
  for (i = 0; i < 7; i++)
    for (j = 0; j < 6; j++) {
      xq->var->setup.macs[i]  [j] = xq->var->write_buffer.msg[(i +   01) + (j * 8)];
      if (xq->var->write_buffer.len > 112)
        xq->var->setup.macs[i+7][j] = xq->var->write_buffer.msg[(i + 0101) + (j * 8)];
    }

  /*
     Under VMS the setup packet that is passed to turn promiscuous 
     off after it has been on doesn't seem to follow the rules documented 
     in both the DEQNA and DELQA manuals.
     These rules seem to say that setup packets less than 128 should only 
     modify the address filter set and probably not the All-Multicast and 
     Promiscuous modes, however, VMS V5-5 and V7.3 seem to send a 127 byte 
     packet to turn this functionality off.  I'm not sure how real hardware 
     behaves in this case, since the only consequence is extra interrupt 
     load.  To realize and retain the benefits of the newly added BPF 
     functionality in sim_ether, I've modified the logic implemented here
     to disable Promiscuous mode when a "small" setup packet is processed.
     I'm deliberately not modifying the All-Multicast mode the same way
     since I don't have an observable case of its behavior.  These two 
     different modes come from very different usage situations:
        1) Promiscuous mode is usually entered for relatively short periods 
           of time due to the needs of a specific application program which
           is doing some sort of management/monitoring function (i.e. tcpdump)
        2) All-Multicast mode is only entered by the OS Kernel Port Driver
           when it happens to have clients (usually network stacks or service 
           programs) which as a group need to listen to more multicast ethernet
           addresses than the 12 (or so) which the hardware supports directly.
     so, I believe that the All-Multicast mode, is first rarely used, and if 
     it ever is used, once set, it will probably be set either forever or for 
     long periods of time, and the additional interrupt processing load to
     deal with the distinctly lower multicast traffic set is clearly lower than
     that of the promiscuous mode.
  */
  xq->var->setup.promiscuous = 0;
  /* process high byte count */
  if (xq->var->write_buffer.len > 128) {
    uint16 len = xq->var->write_buffer.len;
    uint16 led, san;

    xq->var->setup.multicast = (0 != (len & XQ_SETUP_MC));
    xq->var->setup.promiscuous = (0 != (len & XQ_SETUP_PM));
    if (led = (len & XQ_SETUP_LD) >> 2) {
      switch (led) {
        case 1: xq->var->setup.l1 = 0; break;
        case 2: xq->var->setup.l2 = 0; break;
        case 3: xq->var->setup.l3 = 0; break;
      } /* switch */
    } /* if led */

    /* set sanity timer timeout */
    san = (len & XQ_SETUP_ST) >> 4;
    switch(san) {
      case 0: secs = 0.25;    break;  /* 1/4 second  */
      case 1: secs = 1;       break;  /*   1 second  */
      case 2: secs = 4;       break;  /*   4 seconds */
      case 3: secs = 16;      break;  /*  16 seconds */
      case 4: secs =  1 * 60; break;  /*   1 minute  */
      case 5: secs =  4 * 60; break;  /*   4 minutes */
      case 6: secs = 16 * 60; break;  /*  16 minutes */
      case 7: secs = 64 * 60; break;  /*  64 minutes */
    }
    xq->var->sanity.quarter_secs = (int) (secs * 4);
  }

  /* finalize sanity timer state */
  if (xq->var->sanity.enabled != 2) {
    if (xq->var->csr & XQ_CSR_SE)
      xq->var->sanity.enabled = 1;
    else
      xq->var->sanity.enabled = 0;
  }
  xq_reset_santmr(xq);

  /* set ethernet filter */
  /* memcpy (filters[count++], xq->mac, sizeof(ETH_MAC)); */
  for (i = 0; i < XQ_FILTER_MAX; i++)
    if (memcmp(zeros, &xq->var->setup.macs[i], sizeof(ETH_MAC)))
      memcpy (filters[count++], xq->var->setup.macs[i], sizeof(ETH_MAC));
  status = eth_filter (xq->var->etherface, count, filters, xq->var->setup.multicast, xq->var->setup.promiscuous);

  /* process MOP information */
  if (xq->var->write_buffer.msg[0])
    status = xq_process_mop(xq);

  /* mark setup block valid */
  xq->var->setup.valid = 1;

  xq_debug_setup(xq);

  xq->dev->dctrl = saved_debug; /* restore original debugging */

  return SCPE_OK;
}

/*
  Dispatch Write Operation

  The DELQA manual does not explicitly state whether or not multiple packets
  can be written in one transmit operation, so a maximum of 1 packet is assumed.
  
  MP: Hmmm... Figure 3-1 on page 3-3 step 6 says that descriptors will be processed
  until the end of the list is found.

*/
t_stat xq_process_xbdl(CTLR* xq)
{
  RUN_SCOPE;
  const uint16  implicit_chain_status[2] = {XQ_DSC_V | XQ_DSC_C, 1};
  const uint16  write_success[2] = {0, 1 /*Non-Zero TDR*/};
  uint16 b_length, w_length;
  int32 rstatus, wstatus, rwstatus;
  uint32 address;
  t_stat status;

  sim_debug(DBG_TRC, xq->dev, "xq_process_xbdl()\n");

  /* clear write buffer */
  xq->var->write_buffer.len = 0;

  /* process buffer descriptors until not valid */
  while (1) {

    /* Get transmit bdl from memory */
    rwstatus = xq_fetch_bdl_entry(RUN_PASS, xq->var->xbdl_ba, xq->var->xbdl_buf, 10);
    if (rwstatus) return xq_nxm_error(xq);

    /* invalid buffer? */
    if (~xq->var->xbdl_buf[1] & XQ_DSC_V) {
      xq_csr_set_clr(xq, XQ_CSR_XL, 0);
      sim_debug(DBG_WRN, xq->dev, "XBDL List empty\n");
      return SCPE_OK;
    }

    /* compute host memory address */
    address = ((xq->var->xbdl_buf[1] & 0x3F) << 16) | xq->var->xbdl_buf[2];

    /* decode buffer length - two's complement (in words) */
    w_length = ~xq->var->xbdl_buf[3] + 1;
    b_length = w_length * 2;
    if (xq->var->xbdl_buf[1] & XQ_DSC_H) b_length -= 1;
    if (xq->var->xbdl_buf[1] & XQ_DSC_L) b_length -= 1;

    /* explicit chain buffer? */
    if (xq->var->xbdl_buf[1] & XQ_DSC_C) {
      xq->var->xbdl_ba = address;
      sim_debug(DBG_WRN, xq->dev, "XBDL chained buffer encountered: %d\n", b_length);
      continue;
    }

    /* add to transmit buffer, making sure it's not too big */
    if (xq->var->write_buffer.len + b_length > (int) sizeof(xq->var->write_buffer.msg))
      b_length = (uint16) (sizeof(xq->var->write_buffer.msg) - xq->var->write_buffer.len);
    rstatus = Map_ReadB(RUN_PASS, address, b_length, &xq->var->write_buffer.msg[xq->var->write_buffer.len]);
    if (rstatus) return xq_nxm_error(xq);
    xq->var->write_buffer.len += b_length;

    /* end of message? */
    if (xq->var->xbdl_buf[1] & XQ_DSC_E) {
      if (((~xq->var->csr & XQ_CSR_RE) && ((~xq->var->csr & XQ_CSR_IL) || (xq->var->csr & XQ_CSR_EL))) ||  /* loopback */
           (xq->var->xbdl_buf[1] & XQ_DSC_S)) { /* or setup packet (forces loopback regardless of state) */
        if (xq->var->xbdl_buf[1] & XQ_DSC_S) { /* setup packet */
          status = xq_process_setup(xq);

          /* put packet in read buffer */
          ethq_insert (&xq->var->ReadQ, 0, &xq->var->write_buffer, status);
        } else { /* loopback */
          /* put packet in read buffer */
          ethq_insert (&xq->var->ReadQ, 1, &xq->var->write_buffer, 0);
        }

        /* 
         * Ensure update of TX BDL status words is not reordered before reading tx ring and buffer.
         * We do not perform it as driver should check status word 1 flags first, and we do perform
         * memory barrier before writing it in xq_update_bdl_status_words.
         */
        // smp_mb();

        /* update write status */
        wstatus = xq_update_bdl_status_words(RUN_PASS, xq->var->xbdl_ba, (uint16*) write_success);
        if (wstatus) return xq_nxm_error(xq);

        /* clear write buffer */
        xq->var->write_buffer.len = 0;

        /* reset sanity timer */
        xq_reset_santmr(xq);

        /* mark transmission complete */
        xq_csr_set_clr(xq, XQ_CSR_XI, 0);

        /* now trigger "read" of setup or loopback packet */
        if (~xq->var->csr & XQ_CSR_RL)
          status = xq_process_rbdl(xq);

      } else { /* not loopback */

        status = eth_write(xq->var->etherface, &xq->var->write_buffer, xq->var->wcallback);
        if (status != SCPE_OK)           /* not implemented or unattached */
          xq_write_callback(xq, 1);      /* fake failure */
        else {
          if (xq->var->coalesce_latency == 0)
            xq_svc_ex(RUN_PASS, xq->unit[0], xq);        /* service any received data */
        }
        sim_debug(DBG_WRN, xq->dev, "XBDL completed processing write\n");

      } /* loopback/non-loopback */

    } else { /* not at end-of-message */

      sim_debug(DBG_WRN, xq->dev, "XBDL processing implicit chain buffer segment\n");

      /*
       * Ensure update of TX BDL status words is not reordered before reading tx ring and buffer.
       * We do not perform it as driver should check status word 1 flags first, and we do perform
       * memory barrier before writing it in xq_update_bdl_status_words.
       */
      // smp_mb();

      /* update bdl status words */
      wstatus = xq_update_bdl_status_words(RUN_PASS, xq->var->xbdl_ba, (uint16*) implicit_chain_status);
      if(wstatus) return xq_nxm_error(xq);
    }

    /* set to next bdl (implicit chain) */
    xq->var->xbdl_ba += 12;

  } /* while */
}

t_stat xq_dispatch_rbdl(CTLR* xq)
{
  RUN_SCOPE;
  int i;
  int32 rwstatus;
  t_stat status;

  sim_debug(DBG_TRC, xq->dev, "xq_dispatch_rbdl()\n");

  /* mark receive bdl valid */
  xq_csr_set_clr(xq, 0, XQ_CSR_RL);

  /* init receive bdl buffer */
  for (i=0; i<6; i++)
    xq->var->rbdl_buf[i] = 0;

  /* get address of first receive buffer */
  xq->var->rbdl_ba = ((xq->var->rbdl[1] & 0x3F) << 16) | (xq->var->rbdl[0] & ~01);

  /* get first receive buffer */
  rwstatus = xq_fetch_bdl_entry(RUN_PASS, xq->var->rbdl_ba, xq->var->rbdl_buf, 6);
  if (rwstatus) return xq_nxm_error(xq);

  /* is buffer valid? */
  if (~xq->var->rbdl_buf[1] & XQ_DSC_V) {
    xq_csr_set_clr(xq, XQ_CSR_RL, 0);
    return SCPE_OK;
  }

  /* process any waiting packets in receive queue */
  if (xq->var->ReadQ.count)
    status = xq_process_rbdl(xq);

  return SCPE_OK;
}

t_stat xq_dispatch_xbdl(CTLR* xq)
{
  int i;
  t_stat status;

  sim_debug(DBG_TRC, xq->dev, "xq_dispatch_xbdl()\n");

  /* mark transmit bdl valid */
  xq_csr_set_clr(xq, 0, XQ_CSR_XL);

  /* initialize transmit bdl buffers */
  for (i=0; i<6; i++)
    xq->var->xbdl_buf[i] = 0;

  /* clear transmit buffer */
  xq->var->write_buffer.len = 0;

  /* get base address of first transmit descriptor */
  xq->var->xbdl_ba = ((xq->var->xbdl[1] & 0x3F) << 16) | (xq->var->xbdl[0] & ~01);

  /* process xbdl */
  status = xq_process_xbdl(xq);

  return status;
}

t_stat xq_process_turbo_rbdl(CTLR* xq)
{
  RUN_SCOPE;
  int i;
  t_stat status;
  int descriptors_consumed = 0;
  uint32 rdra = (xq->var->init.rdra_h << 16) | xq->var->init.rdra_l;

  sim_debug(DBG_TRC, xq->dev, "xq_process_turbo_rbdl()\n");

  if ((xq->var->srr & XQ_SRR_RESP) != XQ_SRR_STRT)
    return SCPE_OK;

  /* Process descriptors in the receive ring while the're available and we have packets */
  do {
    uint32 address;
    uint16 b_length, rbl;
    ETH_ITEM* item;
    uint8* rbuf;

    /* stop processing when nothing in read queue */
    if (!xq->var->ReadQ.count)
        break;

    i = xq->var->rbindx;

    /* Get receive descriptor from memory */
    status = Map_ReadW (RUN_PASS, rdra+i*sizeof(xq->var->rring[i]), sizeof(xq->var->rring[i]), (uint16 *)&xq->var->rring[i]);
    if (status != SCPE_OK)
        return xq_nxm_error(xq);

    /* Done if Buffer not Owned */
    if (xq->var->rring[i].rmd3 & XQ_TMD3_OWN)
        break;

    ++descriptors_consumed;

    /* Update ring index */
    xq->var->rbindx = (xq->var->rbindx + 1) % XQ_TURBO_RC_BCNT;

    address = ((xq->var->rring[i].hadr & 0x3F ) << 16) | xq->var->rring[i].ladr;
    b_length = ETH_FRAME_SIZE;

    item = &xq->var->ReadQ.item[xq->var->ReadQ.head];
    rbl = item->packet.len + ETH_CRC_SIZE;
    rbuf = item->packet.msg;

    /* see if packet must be size-adjusted or is splitting */
    if (item->packet.used) {
      int used = item->packet.used;
      rbl -= used;
      rbuf = &item->packet.msg[used];
    } else {
      /* adjust runt packets */
      if (rbl < ETH_MIN_PACKET) {
        xq->var->stats.runt += 1;
        sim_debug(DBG_WRN, xq->dev, "Runt detected, size = %d\n", rbl);
        /* pad runts with zeros up to minimum size - this allows "legal" (size - 60)
           processing of those weird short ARP packets that seem to occur occasionally */
        memset(&item->packet.msg[rbl], 0, ETH_MIN_PACKET-rbl);
        rbl = ETH_MIN_PACKET;
      };

      /* adjust oversized packets */
      if (rbl > ETH_FRAME_SIZE) {
        xq->var->stats.giant += 1;
        sim_debug(DBG_WRN, xq->dev, "Giant detected, size=%d\n", rbl);
        /* trim giants down to maximum size - no documentation on how to handle the data loss */
        item->packet.len = ETH_MAX_PACKET;
        rbl = ETH_FRAME_SIZE;
      };
    };

    /* make sure entire packet fits in buffer - if not, will need to split into multiple buffers */
    if (rbl > b_length)
      rbl = b_length;
    item->packet.used += rbl;
    
    /* send data to host */
    status = Map_WriteB(RUN_PASS, address, rbl, rbuf);
    if (status != SCPE_OK)
      return xq_nxm_error(xq);

    /* set receive size into RBL - RBL<10:8> maps into Status1<10:8>,
       RBL<7:0> maps into Status2<7:0>, and Status2<15:8> (copy) */
    xq->var->rring[i].rmd0 = 0;
    xq->var->rring[i].rmd1 = rbl;
    xq->var->rring[i].rmd2 = XQ_RMD2_RON | XQ_RMD2_TON;
    if (0 == (item->packet.used - rbl))
      xq->var->rring[i].rmd0 |= XQ_RMD0_STP; /* Start of Packet */
    if (item->packet.used == (item->packet.len + ETH_CRC_SIZE))
      xq->var->rring[i].rmd0 |= XQ_RMD0_ENP; /* End of Packet */
    
    if (xq->var->ReadQ.loss) {
      xq->var->rring[i].rmd2 |= XQ_RMD2_MIS; 
      sim_debug(DBG_WRN, xq->dev, "ReadQ overflow!\n");
      xq->var->stats.dropped += xq->var->ReadQ.loss;
      xq->var->ReadQ.loss = 0;          /* reset loss counter */
    }

    Map_ReadW (RUN_PASS, rdra+(uint32)(((char *)(&xq->var->rring[xq->var->rbindx].rmd3))-((char *)&xq->var->rring)), sizeof(xq->var->rring[xq->var->rbindx].rmd3), (uint16 *)&xq->var->rring[xq->var->rbindx].rmd3);
    if (xq->var->rring[xq->var->rbindx].rmd3 & XQ_RMD3_OWN)
      xq->var->rring[i].rmd2 |= XQ_RMD2_EOR;

    /* Update receive descriptor in memory (only done after we've processed the contents) */
    /* Note: We're updating all but the end of the descriptor (which we never change) */
    /*       AND the driver will be allowed to change once the changed tmd3 (ownership) */
    /*       is noted so we avoid walking on its changes */
    xq->var->rring[i].rmd3 |= XQ_TMD3_OWN; /* Return Descriptor to Driver */
    status = Map_WriteW (RUN_PASS, rdra+i*sizeof(xq->var->rring[i]), sizeof(xq->var->rring[i])-8, (uint16 *)&xq->var->rring[i]);
    if (status != SCPE_OK)
      return xq_nxm_error(xq);

    /* remove packet from queue */
    if (item->packet.used >= item->packet.len)
      ethq_remove(&xq->var->ReadQ);
  } while (0 == (xq->var->rring[xq->var->rbindx].rmd3 & XQ_RMD3_OWN));

  if (xq->var->rring[xq->var->rbindx].rmd3 & XQ_RMD3_OWN)
      sim_debug(DBG_WRN, xq->dev, "xq_process_turbo_rbdl() - receive ring full\n");

  if (descriptors_consumed)
    /* Interrupt for Packet Reception Completion */
    xq_setint(xq);

  return SCPE_OK;
}

t_stat xq_process_turbo_xbdl(CTLR* xq)
{
  RUN_SCOPE;
  int i;
  t_stat status;
  int descriptors_consumed  = 0;
  uint32 tdra = (xq->var->init.tdra_h << 16) | xq->var->init.tdra_l;

  sim_debug(DBG_TRC, xq->dev, "xq_process_turbo_xbdl()\n");

  if ((xq->var->srr & XQ_SRR_RESP) != XQ_SRR_STRT)
    return SCPE_OK;

  /* clear transmit buffer */
  xq->var->write_buffer.len = 0;

  /* Process each descriptor in the transmit ring */
  do {
    uint32 address;
    uint16 b_length;

    i = xq->var->tbindx;

    /* Get transmit descriptor from memory */
    status = Map_ReadW (RUN_PASS, tdra+i*sizeof(xq->var->xring[i]), sizeof(xq->var->xring[i]), (uint16 *)&xq->var->xring[i]);
    if (status != SCPE_OK)
      return xq_nxm_error(xq);

    if (xq->var->xring[i].tmd3 & XQ_TMD3_OWN)
        break;

    /* Update ring index */
    xq->var->tbindx = (xq->var->tbindx + 1) % XQ_TURBO_XM_BCNT;

    ++descriptors_consumed;
    address = ((xq->var->xring[i].hadr & 0x3F ) << 16) | xq->var->xring[i].ladr;
    b_length = (xq->var->xring[i].tmd3 & XQ_TMD3_BCT);

    /* add to transmit buffer, making sure it's not too big */
    if (xq->var->write_buffer.len + b_length > (int) sizeof(xq->var->write_buffer.msg))
      b_length = (uint16)(sizeof(xq->var->write_buffer.msg) - xq->var->write_buffer.len);
    status = Map_ReadB(RUN_PASS, address, b_length, &xq->var->write_buffer.msg[xq->var->write_buffer.len]);
    if (status != SCPE_OK)
      return xq_nxm_error(xq);

    xq->var->write_buffer.len += b_length;
    if (!(xq->var->xring[i].tmd3 & XQ_TMD3_FOT)) {
      /* Process Loopback if in Loopback mode */
      if (xq->var->init.mode & XQ_IN_MO_LOP) {
        if ((xq->var->init.mode & XQ_IN_MO_INT) || (xq->var->etherface)) {
          /* put packet in read buffer */
          ethq_insert (&xq->var->ReadQ, 1, &xq->var->write_buffer, 0);
          status = SCPE_OK;
        } else {
          /* External loopback fails when not connected */
          status = SCPE_NOFNC;
        }
      } else
        status = eth_write(xq->var->etherface, &xq->var->write_buffer, NULL);

      xq->var->stats.xmit += 1;
      if (status != SCPE_OK) {         /* not implemented or unattached */
        sim_debug(DBG_WRN, xq->dev, "Packet Write Error!\n");
        xq->var->stats.fail += 1;
        xq->var->xring[i].tmd0 = XQ_TMD0_ERR1;
        xq->var->xring[i].tmd1 = 100 + xq->var->write_buffer.len * 8; /* arbitrary value */
        xq->var->xring[i].tmd1 |= XQ_TMD1_LCA;
      } else {
        if (DBG_PCK & xq->dev->dctrl)
          eth_packet_trace_ex(xq->var->etherface, xq->var->write_buffer.msg, xq->var->write_buffer.len, "xq-write", DBG_DAT & xq->dev->dctrl, DBG_PCK);
        xq->var->xring[i].tmd0 = 0;
        xq->var->xring[i].tmd1 = 100 + xq->var->write_buffer.len * 8; /* arbitrary value */
      }
      sim_debug(DBG_WRN, xq->dev, "XBDL completed processing write\n");
      /* clear transmit buffer */
      xq->var->write_buffer.len = 0;
      xq->var->xring[i].tmd2 = XQ_TMD2_RON | XQ_TMD2_TON;
    }

    Map_ReadW (RUN_PASS, tdra+(uint32)(((char *)(&xq->var->xring[xq->var->tbindx].tmd3))-((char *)&xq->var->xring)), sizeof(xq->var->xring[xq->var->tbindx].tmd3), (uint16 *)&xq->var->xring[xq->var->tbindx].tmd3);
    if (xq->var->xring[xq->var->tbindx].tmd3 & XQ_TMD3_OWN)
      xq->var->xring[i].tmd2 |= XQ_TMD2_EOR;

    /* Update transmit descriptor in memory (only done after we've processed the contents) */
    /* Note: We're updating all but the end of the descriptor (which we never change) */
    /*       AND the driver will be allowed to change once the changed tmd3 (ownership) */
    /*       is noted so we avoid walking on its changes */
    xq->var->xring[i].tmd3 |= XQ_TMD3_OWN; /* Return Descriptor to Driver */
    status = Map_WriteW (RUN_PASS, tdra+i*sizeof(xq->var->xring[i]), sizeof(xq->var->xring[i])-8, (uint16 *)&xq->var->xring[i]);
    if (status != SCPE_OK)
      return xq_nxm_error(xq);

  } while (0 == (xq->var->xring[xq->var->tbindx].tmd3 & XQ_TMD3_OWN));

  if (descriptors_consumed) {

    /* Interrupt for Packet Transmission Completion */
    xq_setint(xq);

    if (xq->var->coalesce_latency == 0)
      xq_svc_ex (RUN_PASS, xq->unit[0], xq);        /* service any received data */
  } else {
    /* There appears to be a bug in the VMS SCS/XQ driver when it uses chained
       buffers to transmit a packet.  It updates the transmit buffer ring in the
       correct order (i.e. clearing the ownership on the last packet segment 
       first), but it writes a transmit request to the ARQR register after adjusting
       the ownership of EACH buffer piece.  This results in us being awakened once
       and finding nothing to do.  We ignore this and the next write the ARQR will
       properly cause the packet transmission.
     */
    sim_debug(DBG_WRN, xq->dev, "xq_process_turbo_xbdl() - Nothing to Transmit\n");
  }

  return status;
}

t_stat xq_process_loopback(CTLR* xq, ETH_PACK* pack)
{
  ETH_PACK  response;
  ETH_MAC   *physical_address;
  t_stat    status;
  int offset   = 16 + (pack->msg[14] | (pack->msg[15] << 8));
  int function = pack->msg[offset] | (pack->msg[offset+1] << 8);

  sim_debug(DBG_TRC, xq->dev, "xq_process_loopback()\n");

  if (function != 2 /*forward*/)
    return SCPE_NOFNC;

  /* create forward response packet */
  memcpy (&response, pack, sizeof(ETH_PACK));
  if (xq->var->mode == XQ_T_DELQA_PLUS)
    physical_address = &xq->var->init.phys;
  else
      if (xq->var->setup.valid)
        physical_address = &xq->var->setup.macs[0];
      else
        physical_address = &xq->var->mac;

  /* The only packets we should be responding to are ones which 
     we received due to them being directed to our physical MAC address, 
     OR the Broadcast address OR to a Multicast address we're listening to 
     (we may receive others if we're in promiscuous mode, but shouldn't 
     respond to them) */
  if ((0 == (pack->msg[0]&1)) &&           /* Multicast or Broadcast */
      (0 != memcmp(physical_address, pack->msg, sizeof(ETH_MAC))))
      return SCPE_NOFNC;

  memcpy (&response.msg[0], &response.msg[offset+2], sizeof(ETH_MAC));
  memcpy (&response.msg[6], physical_address, sizeof(ETH_MAC));
  offset += 8 - 16; /* Account for the Ethernet Header and Offset value in this number  */
  response.msg[14] = offset & 0xFF;
  response.msg[15] = (offset >> 8) & 0xFF;

  /* send response packet */
  status = eth_write(xq->var->etherface, &response, NULL);
  ++xq->var->stats.loop;

  if (DBG_PCK & xq->dev->dctrl)
      eth_packet_trace_ex(xq->var->etherface, response.msg, response.len, ((function == 1) ? "xq-loopbackreply" : "xq-loopbackforward"), DBG_DAT & xq->dev->dctrl, DBG_PCK);

  return status;
}

t_stat xq_process_remote_console (CTLR* xq, ETH_PACK* pack)
{
  t_stat status;
  ETH_MAC source;
  uint16 receipt;
  int code = pack->msg[16];

  sim_debug(DBG_TRC, xq->dev, "xq_process_remote_console()\n");

  switch (code) {
    case 0x05: /* request id */
      receipt = pack->msg[18] | (pack->msg[19] << 8);
      memcpy(source, &pack->msg[6], sizeof(ETH_MAC));

      /* send system id to requestor */
      status = xq_system_id (xq, source, receipt);
      return status;
      break;
    case 0x06:  /* boot */
      /*
      NOTE: the verification field should be checked here against the
      verification value established in the setup packet. If they match the
      reboot should occur, otherwise nothing happens, and the packet
      is passed on to the host.

      Verification is not implemented, since the setup packet processing code
      isn't complete yet.

      Various values are also passed: processor, control, and software id.
      These control the various boot parameters, however SIMH does not
      have a mechanism to pass these to the host, so just reboot.
      */

      status = xq_boot_host(xq);
      return status;
      break;
  } /* switch */

  return SCPE_NOFNC;
}

t_stat xq_process_local (CTLR* xq, ETH_PACK* pack)
{
  /* returns SCPE_OK if local processing occurred,
     otherwise returns SCPE_NOFNC or some other code */
  int protocol;

  sim_debug(DBG_TRC, xq->dev, "xq_process_local()\n");
  /* DEQNA's have no local processing capability */
  if (xq->var->type == XQ_T_DEQNA)
    return SCPE_NOFNC;

  protocol = pack->msg[12] | (pack->msg[13] << 8);
  switch (protocol) {
    case 0x0090:  /* ethernet loopback */
      return xq_process_loopback(xq, pack);
      break;
    case 0x0260:  /* MOP remote console */
      return xq_process_remote_console(xq, pack);
      break;
  }
  return SCPE_NOFNC;
}

void xq_read_callback(CTLR* xq, int status)
{
  xq->var->stats.recv += 1;

  if (DBG_PCK & xq->dev->dctrl)
    eth_packet_trace_ex(xq->var->etherface, xq->var->read_buffer.msg, xq->var->read_buffer.len, "xq-recvd", DBG_DAT & xq->dev->dctrl, DBG_PCK);

  if ((xq->var->csr & XQ_CSR_RE) || (xq->var->mode == XQ_T_DELQA_PLUS)) { /* receiver enabled */

    /* process any packets locally that can be */
    t_stat status = xq_process_local (xq, &xq->var->read_buffer);

    /* add packet to read queue */
    if (status != SCPE_OK)
      ethq_insert(&xq->var->ReadQ, 2, &xq->var->read_buffer, status);
  } else {
    xq->var->stats.dropped += 1;
    sim_debug(DBG_WRN, xq->dev, "packet received with receiver disabled\n");
  }
}

void xqa_read_callback(int status)
{
  CTLR* xq = &xq_ctrl[0];
  AUTO_LOCK_NM(xq_autolock, *xq->xq_lock);
  xq_read_callback(xq, status);
}

void xqb_read_callback(int status)
{
  CTLR* xq = &xq_ctrl[1];
  AUTO_LOCK_NM(xq_autolock, *xq->xq_lock);
  xq_read_callback(xq, status);
}

void xq_sw_reset(CTLR* xq)
{
  const uint16 set_bits = XQ_CSR_XL | XQ_CSR_RL;
  int i;

  sim_debug(DBG_TRC, xq->dev, "xq_sw_reset()\n");
  ++xq->var->stats.reset;

  /* Return DELQA-T to DELQA Normal mode */
  if (xq->var->type == XQ_T_DELQA_PLUS) {
    xq->var->mode = XQ_T_DELQA;
    xq->var->iba = xq->var->srr = 0;
  }

  /* reset csr bits */
  xq_csr_set_clr(xq, set_bits, (uint16) ~set_bits);

  if (xq->var->etherface)
    xq_csr_set_clr(xq, XQ_CSR_OK, 0);

  /* clear interrupt unconditionally */
  xq_clrint(xq);

  /* flush read queue */
  ethq_clear(&xq->var->ReadQ);

  /* clear setup info */
  xq->var->setup.multicast = 0;
  xq->var->setup.promiscuous = 0;
  if (xq->var->etherface) {
    int count = 0;
    ETH_MAC zeros = {0, 0, 0, 0, 0, 0};
    ETH_MAC filters[XQ_FILTER_MAX + 1];

    /* set ethernet filter */
    /* memcpy (filters[count++], xq->mac, sizeof(ETH_MAC)); */
    for (i = 0; i < XQ_FILTER_MAX; i++)
      if (memcmp(zeros, &xq->var->setup.macs[i], sizeof(ETH_MAC)))
        memcpy (filters[count++], xq->var->setup.macs[i], sizeof(ETH_MAC));
    eth_filter (xq->var->etherface, count, filters, xq->var->setup.multicast, xq->var->setup.promiscuous);
    }

  /* Stop receive polling until the receiver is reenabled */
  xq_stop_receiver(xq);

}

/* write registers: */

t_stat xq_wr_var(CTLR* xq, int32 data)
{
  uint16 save_var = xq->var->var;
  sim_debug(DBG_REG, xq->dev, "xq_wr_var(data= 0x%08X\n", data);
  
  switch (xq->var->type) {
    case XQ_T_DEQNA:
      xq->var->var = (data & XQ_VEC_IV);
      break;
    case XQ_T_DELQA:
    case XQ_T_DELQA_PLUS:
      xq->var->var = (xq->var->var & XQ_VEC_RO) | (data & XQ_VEC_RW);

      /* if switching to DEQNA-LOCK mode clear VAR<14:10> */
      if (~xq->var->var & XQ_VEC_MS) {
        xq->var->mode = XQ_T_DEQNA;
        xq->var->var &= ~(XQ_VEC_OS | XQ_VEC_RS | XQ_VEC_ST);
      } else {
        xq->var->mode = XQ_T_DELQA;
      }

      /* if Self Test is on, turn it off to signal completion */
      if (xq->var->var & XQ_VEC_RS) {
        xq->var->var &= ~XQ_VEC_RS;
        if (!xq->var->etherface)
          xq->var->var |= XQ_VEC_S1; /* Indicate No Network Connection */
        else
          xq->var->var &= ~XQ_VEC_ST; /* Set success Status */
      }
      break;
  }

  /* set vector of SIMH device */
  if (data & XQ_VEC_IV)
  {
    io_change_vec(xq->dib, (data & XQ_VEC_IV) + VEC_Q);
  }
  else
  {
    io_change_vec(xq->dib, 0);
  }

  sim_debug_u16(DBG_VAR, xq->dev, xq_var_bits, save_var, xq->var->var, 1);

  return SCPE_OK;
}

#ifdef VM_PDP11
t_stat xq_process_bootrom (CTLR* xq)
{
  /*
  NOTE: BOOT ROMs are a PDP-11ism, since they contain PDP-11 binary code.
        the host is responsible for creating two *2KB* receive buffers.

  RSTS/E v10.1 source (INIONE.MAR/XHLOOK:) indicates that both the DEQNA and
  DELQA will set receive status word 1 bits 15 & 14 on both packets. It also
  states that a hardware bug in the DEQNA will set receive status word 1 bit 15
  (only) in the *third* receive buffer (oops!).

  RSTS/E v10.1 will run the Citizenship test from the bootrom after loading it.
  Documentation on the Boot ROM can be found in INIQNA.MAR.
  */

  int32 rstatus, wstatus;
  uint16 b_length, w_length;
  uint32 address;
  uint8*  bootrom = (uint8*) xq_bootrom;
  int     i, checksum;

  sim_debug(DBG_TRC, xq->dev, "xq_process_bootrom()\n");

  /*
  RSTS/E v10.1 invokes the Citizenship tests in the Bootrom. For some
  reason, the current state of the XQ emulator cannot pass these. So,
  to get moving on RSTE/E support, we will replace the following line in
  INIQNA.MAR/CITQNA::
    70$: MOV (R2),R0 ;get the status word
  with
    70$: CLR R0      ;force success
  to cause the Citizenship test to return success to RSTS/E.

  At some point, the real problem (failure to pass citizenship diagnostics)
  does need to be corrected to find incompatibilities in the emulation, and to
  ultimately allow it to pass Digital hardware diagnostic tests.
  */
  for (i=0; i<sizeof(xq_bootrom)/2; i++)
    if (xq_bootrom[i] == 011200) {  /* MOV (R2),R0 */
      xq_bootrom[i] = 005000;       /* CLR R0 */
      break;
    }

  /* recalculate checksum, which is a simple byte sum */
  for (i=0, checksum=0; i<sizeof(xq_bootrom)-2; i++)
    checksum += bootrom[i];

  /* set new checksum */
  xq_bootrom[sizeof(xq_bootrom)/2-1] = checksum;

  /* --------------------------- bootrom part 1 -----------------------------*/

  /* get receive bdl from memory */
  xq->var->rbdl_buf[0] = 0xFFFF;
  wstatus = Map_WriteW(RUN_PASS, xq->var->rbdl_ba,     2, &xq->var->rbdl_buf[0]);
  rstatus = Map_ReadW (RUN_PASS, xq->var->rbdl_ba + 2, 6, &xq->var->rbdl_buf[1]);
  if (rstatus || wstatus) return xq_nxm_error(xq);

  /* invalid buffer? */
  if (~xq->var->rbdl_buf[1] & XQ_DSC_V) {
    xq_csr_set_clr(xq, XQ_CSR_RL, 0);
    return SCPE_OK;
  }

  /* get status words */
  rstatus = Map_ReadW(RUN_PASS, xq->var->rbdl_ba + 8, 4, &xq->var->rbdl_buf[4]);
  if (rstatus) return xq_nxm_error(xq);

  /* get host memory address */
  address = ((xq->var->rbdl_buf[1] & 0x3F) << 16) | xq->var->rbdl_buf[2];

  /* decode buffer length - two's complement (in words) */
  w_length = ~xq->var->rbdl_buf[3] + 1;
  b_length = w_length * 2;
  if (xq->var->rbdl_buf[1] & XQ_DSC_H) b_length -= 1;
  if (xq->var->rbdl_buf[1] & XQ_DSC_L) b_length -= 1;

  /* make sure entire packet fits in buffer */
  assert(b_length >= sizeof(xq_bootrom)/2);

  /* send data to host */
  wstatus = Map_WriteB(RUN_PASS, address, sizeof(xq_bootrom)/2, bootrom);
  if (wstatus) return xq_nxm_error(xq);

  /* update read status words */
  xq->var->rbdl_buf[4] = XQ_DSC_V | XQ_DSC_C;   /* valid, chain */
  xq->var->rbdl_buf[5] = 0;

  /* update read status words*/
  wstatus = Map_WriteW(RUN_PASS, xq->var->rbdl_ba + 8, 4, &xq->var->rbdl_buf[4]);
  if (wstatus) return xq_nxm_error(xq);

  /* set to next bdl (implicit chain) */
  xq->var->rbdl_ba += 12;

  /* --------------------------- bootrom part 2 -----------------------------*/

  /* get receive bdl from memory */
  xq->var->rbdl_buf[0] = 0xFFFF;
  wstatus = Map_WriteW(RUN_PASS, xq->var->rbdl_ba,     2, &xq->var->rbdl_buf[0]);
  rstatus = Map_ReadW (RUN_PASS, xq->var->rbdl_ba + 2, 6, &xq->var->rbdl_buf[1]);
  if (rstatus || wstatus) return xq_nxm_error(xq);

  /* invalid buffer? */
  if (~xq->var->rbdl_buf[1] & XQ_DSC_V) {
    xq_csr_set_clr(xq, XQ_CSR_RL, 0);
    return SCPE_OK;
  }

  /* get status words */
  rstatus = Map_ReadW(RUN_PASS, xq->var->rbdl_ba + 8, 4, &xq->var->rbdl_buf[4]);
  if (rstatus) return xq_nxm_error(xq);

  /* get host memory address */
  address = ((xq->var->rbdl_buf[1] & 0x3F) << 16) | xq->var->rbdl_buf[2];

  /* decode buffer length - two's complement (in words) */
  w_length = ~xq->var->rbdl_buf[3] + 1;
  b_length = w_length * 2;
  if (xq->var->rbdl_buf[1] & XQ_DSC_H) b_length -= 1;
  if (xq->var->rbdl_buf[1] & XQ_DSC_L) b_length -= 1;

  /* make sure entire packet fits in buffer */
  assert(b_length >= sizeof(xq_bootrom)/2);

  /* send data to host */
  wstatus = Map_WriteB(RUN_PASS, address, sizeof(xq_bootrom)/2, &bootrom[2048]);
  if (wstatus) return xq_nxm_error(xq);

  /* update read status words */
  xq->var->rbdl_buf[4] = XQ_DSC_V | XQ_DSC_C;   /* valid, chain */
  xq->var->rbdl_buf[5] = 0;

  /* update read status words*/
  wstatus = Map_WriteW(RUN_PASS, xq->var->rbdl_ba + 8, 4, &xq->var->rbdl_buf[4]);
  if (wstatus) return xq_nxm_error(xq);

  /* set to next bdl (implicit chain) */
  xq->var->rbdl_ba += 12;

  /* --------------------------- bootrom part 3 -----------------------------*/

  switch (xq->var->type) {
    case XQ_T_DEQNA:

      /* get receive bdl from memory */
      xq->var->rbdl_buf[0] = 0xFFFF;
      wstatus = Map_WriteW(RUN_PASS, xq->var->rbdl_ba,     2, &xq->var->rbdl_buf[0]);
      rstatus = Map_ReadW (RUN_PASS, xq->var->rbdl_ba + 2, 6, &xq->var->rbdl_buf[1]);
      if (rstatus || wstatus) return xq_nxm_error(xq);

      /* invalid buffer? */
      if (~xq->var->rbdl_buf[1] & XQ_DSC_V) {
        xq_csr_set_clr(xq, XQ_CSR_RL, 0);
        return SCPE_OK;
      }

      /* get status words */
      rstatus = Map_ReadW(RUN_PASS, xq->var->rbdl_ba + 8, 4, &xq->var->rbdl_buf[4]);
      if (rstatus) return xq_nxm_error(xq);

      /* update read status words */
      xq->var->rbdl_buf[4] = XQ_DSC_V;   /* valid */
      xq->var->rbdl_buf[5] = 0;

      /* update read status words*/
      wstatus = Map_WriteW(RUN_PASS, xq->var->rbdl_ba + 8, 4, &xq->var->rbdl_buf[4]);
      if (wstatus) return xq_nxm_error(xq);

      /* set to next bdl (implicit chain) */
      xq->var->rbdl_ba += 12;
      break;
  } /* switch */

  /* --------------------------- Done, finish up -----------------------------*/

  /* mark transmission complete */
  xq_csr_set_clr(xq, XQ_CSR_RI, 0);

  /* reset sanity timer */
  xq_reset_santmr(xq);

  return SCPE_OK;
}
#endif /* ifdef VM_PDP11 */

t_stat xq_wr_csr(CTLR* xq, int32 data)
{
  uint16 set_bits = data & XQ_CSR_RW;                      /* set RW set bits */
  uint16 clr_bits = ((data ^ XQ_CSR_RW) & XQ_CSR_RW)       /* clear RW cleared bits */
                  |  (data & XQ_CSR_W1)                    /* write 1 to clear bits */
                  | ((data & XQ_CSR_XI) ? XQ_CSR_NI : 0);  /* clearing XI clears NI */

  sim_debug(DBG_REG, xq->dev, "xq_wr_csr(data=0x%08X)\n", data);

  /* reset controller when SR transitions to cleared */
  if (xq->var->csr & XQ_CSR_SR & ~data) {
    xq_sw_reset(xq);
    return SCPE_OK;
  }

  /* start receiver when RE transitions to set */
  if (~xq->var->csr & XQ_CSR_RE & data) {
    sim_debug(DBG_REG, xq->dev, "xq_wr_csr(data=0x%08X) - receiver started\n", data);

    /* start the read service timer or enable asynch reading as appropriate */
    xq_start_receiver(xq);
  }

  /* stop receiver when RE transitions to clear */
  if (xq->var->csr & XQ_CSR_RE & ~data) {
    sim_debug(DBG_REG, xq->dev, "xq_wr_csr(data=0x%08X) - receiver stopped\n", data);

    /* stop the read service timer or disable asynch reading as appropriate */
    xq_stop_receiver(xq);
  }

  /* update CSR bits */
  xq_csr_set_clr (xq, set_bits, clr_bits);

#ifdef VM_PDP11
  /* request boot/diagnostic rom? [PDP-11 only] */
  if ((xq->var->csr & XQ_CSR_BP) == XQ_CSR_BP)  /* all bits must be on */
    xq_process_bootrom(xq);
#endif

  return SCPE_OK;
}

void xq_start_receiver(CTLR* xq)
{
  if (!xq->var->etherface)
    return;

  /* start the read service timer or enable asynch reading as appropriate */
  if (xq->var->must_poll)
  {
    xq_activate(xq->unit[0], TRUE, xq->var->poll);
  }
  else
  {
    if (xq->var->poll == 0 || xq->var->mode == XQ_T_DELQA_PLUS)
      eth_set_async(xq->var->etherface, xq->var->coalesce_latency_ticks);
    else
      xq_activate(xq->unit[0], TRUE, xq->var->poll);
  }
}

void xq_stop_receiver(CTLR* xq)
{
  sim_cancel(xq->unit[0]);  /* Stop Receiving */
  if (xq->var->etherface)
    eth_clr_async(xq->var->etherface);
}

t_stat xq_wr_srqr(CTLR* xq, int32 data)
{
  RUN_SCOPE;
  uint16 set_bits = data & XQ_SRQR_RW;                     /* set RW set bits */

  sim_debug(DBG_REG, xq->dev, "xq_wr_srqr(data=0x%08X)\n", data);

  xq->var->srr = set_bits;

  switch (set_bits) {
    case XQ_SRQR_STRT: {
      t_stat status;

      xq->var->stats.setup += 1;
      /* Get init block from memory */
      status = Map_ReadW (RUN_PASS, xq->var->iba, sizeof(xq->var->init), (uint16 *)&xq->var->init);
      if (SCPE_OK != status) {
        xq_nxm_error (xq);
      } else {
        uint32 saved_debug = xq->dev->dctrl;

        /* temporarily turn on Ethernet debugging if setup debugging is enabled */
        if (xq->dev->dctrl & DBG_SET)
          xq->dev->dctrl |= DBG_ETH;

        xq_debug_turbo_setup(xq);

        xq->dib->vec = xq->var->init.vector + VEC_Q;
        xq->var->tbindx = xq->var->rbindx = 0;
        if ((xq->var->sanity.enabled) && (xq->var->init.options & XQ_IN_OP_HIT)) {
          xq->var->sanity.quarter_secs = 4*xq->var->init.hit_timeout;
        }
        xq->var->icr = xq->var->init.options & XQ_IN_OP_INT;
        status = eth_filter_hash (xq->var->etherface, 1, &xq->var->init.phys, 0, xq->var->init.mode & XQ_IN_MO_PRO, &xq->var->init.hash_filter);

        xq->dev->dctrl = saved_debug; /* restore original debugging */
      }
      /* start the read service timer or enable asynch reading as appropriate */
      xq_start_receiver(xq);
      break;
      }
    case XQ_SRQR_STOP:
        xq_stop_receiver(xq);
      break;
    default:
      break;
  }

  /* All Writes to SRQR reset the Host Inactivity Timer */
  xq_reset_santmr(xq);

  /* Interrupt after this synchronous request completion */
  xq_setint(xq);

  return SCPE_OK;
}

t_stat xq_wr_arqr(CTLR* xq, int32 data)
{
  sim_debug(DBG_REG, xq->dev, "xq_wr_arqr(data=0x%08X)\n", data);

  /* initiate transmit activity when requested */
  if (XQ_ARQR_TRQ & data) {
    xq_process_turbo_xbdl (xq);
  }
  /* initiate transmit activity when requested */
  if (XQ_ARQR_RRQ & data) {
    xq_process_turbo_rbdl (xq);
  }

  /* reset controller when requested */
  if (XQ_ARQR_SR & data) {
    xq_sw_reset(xq);
  }

  /* All Writes to ARQR reset the Host Inactivity Timer */
  xq_reset_santmr(xq);

  return SCPE_OK;
}

t_stat xq_wr_icr(CTLR* xq, int32 data)
{
  uint16 old_icr = xq->var->icr;

  sim_debug(DBG_REG, xq->dev, "xq_wr_icr(data=0x%08X)\n", data);

  xq->var->icr = data & XQ_ICR_ENA;

  if (xq->var->icr && !old_icr && xq->var->pending_interrupt)
    xq_setint(xq);

  return SCPE_OK;
}

t_stat xq_wr(int32 data, int32 PA, int32 access)
{
  t_stat status;
  CTLR* xq = xq_pa2ctlr(PA);
  AUTO_LOCK_NM(xq_autolock, *xq->xq_lock);
  int index = (PA >> 1) & 07;   /* word index */

  sim_debug(DBG_REG, xq->dev, "xq_wr(data=0x%08X, PA=0x%08X[%s], access=%d)\n", data, PA, ((xq->var->mode == XQ_T_DELQA_PLUS) ? xqt_xmit_regnames[index] : xq_xmit_regnames[index]), access);

  switch (xq->var->mode) {
    case XQ_T_DELQA_PLUS:
      switch (index) {
        case 0:   /* IBAL */
          xq->var->iba = (xq->var->iba & 0xFFFF0000) | (data & 0xFFFF);
          break;
        case 1:   /* IBAH */
          xq->var->iba = (xq->var->iba & 0xFFFF) | ((data & 0xFFFF) << 16);
          break;
        case 2:   /* ICR */
          status = xq_wr_icr(xq, data);
          break;
        case 3:
          break;
        case 4:   /* SRQR */
          status = xq_wr_srqr(xq, data);
          break;
        case 5:
          break;
        case 6:
          break;
        case 7:   /* ARQR */
          status = xq_wr_arqr(xq, data);
          break;
      }
      break;
    default: /* DEQNA, DELQA Normal */
      switch (index) {
        case 0:   /* IBAL/XCR0 */ /* these should only be written on a DELQA-T */
          if (xq->var->type == XQ_T_DELQA_PLUS)
            xq->var->iba = (xq->var->iba & 0xFFFF0000) | (data & 0xFFFF);
          break;
        case 1:   /* IBAH/XCR1 */
          if (xq->var->type == XQ_T_DELQA_PLUS) {
            if (((xq->var->iba & 0xFFFF) == 0x0BAF) && (data == 0xFF00)) {
              xq->var->mode = XQ_T_DELQA_PLUS;
              xq->var->srr = XQ_SRR_TRBO;
              sim_cancel(xq->unit[0]); /* Turn off receive processing until explicitly enabled */
              eth_clr_async(xq->var->etherface);
            }
            xq->var->iba = (xq->var->iba & 0xFFFF) | ((data & 0xFFFF) << 16);
          }
          break;
        case 2:   /* receive bdl low bits */
          xq->var->rbdl[0] = data;
          break;
        case 3:   /* receive bdl high bits */
          xq->var->rbdl[1] = data;
          status = xq_dispatch_rbdl(xq); /* start receive operation */
          break;
        case 4:   /* transmit bdl low bits */
          xq->var->xbdl[0] = data;
          break;
        case 5:   /* transmit bdl high bits */
          xq->var->xbdl[1] = data;
          status = xq_dispatch_xbdl(xq); /* start transmit operation */
          break;
        case 6:   /* vector address register */
          status = xq_wr_var(xq, data);
          break;
        case 7:   /* control and status register */
          status = xq_wr_csr(xq, data);
          break;
      }
      break;
  }
  return SCPE_OK;
}


/* reset device */
t_stat xq_reset(DEVICE* dptr)
{
  t_stat status;
  CTLR* xq = xq_dev2ctlr(dptr);
  AUTO_LOCK_NM(xq_autolock, *xq->xq_lock);
  const uint16 set_bits = XQ_CSR_RL | XQ_CSR_XL;

  sim_bind_devunits_lock(dptr, *xq->xq_lock);

  sim_debug(DBG_TRC, xq->dev, "xq_reset()\n");

  /* calculate MAC checksum */
  xq_make_checksum(xq);

  /* init vector address register */
  switch (xq->var->type) {
    case XQ_T_DEQNA:
      xq->var->var = 0;
      xq->var->mode = XQ_T_DEQNA;
      break;
    case XQ_T_DELQA:
    case XQ_T_DELQA_PLUS:
      xq->var->var = XQ_VEC_MS | XQ_VEC_OS;
      xq->var->mode = XQ_T_DELQA;
      break;
  }
  io_change_vec(xq->dib, 0);

  /* init control status register */
  xq_csr_set_clr(xq, set_bits, (uint16) ~set_bits);

  /* clear interrupts unconditionally */
  xq_clrint(xq);

  /* init read queue (first time only) */
  status = ethq_init(&xq->var->ReadQ, XQ_QUE_MAX);
  if (status != SCPE_OK)
    return status;

  /* clear read queue */
  ethq_clear(&xq->var->ReadQ);

  /* reset ethernet interface */
  if (xq->var->etherface) {
    /* restore filter on ROM mac address */
    status = eth_filter (xq->var->etherface, 1, &xq->var->mac, 0, 0);
    xq_csr_set_clr(xq, XQ_CSR_OK, 0);

    /* start service timer */
    xq_activate_abs(xq->unit[1], FALSE, 4);

    /* stop the receiver */
    eth_clr_async(xq->var->etherface);
  }

  /* stop the receiver */
  sim_cancel(xq->unit[0]);

  /* set hardware sanity controls */
  if (xq->var->sanity.enabled) {
    xq->var->sanity.quarter_secs = XQ_HW_SANITY_SECS * 4/*qsec*/;
  }

  return auto_config (0, 0);                              /* run autoconfig */
}

void xq_reset_santmr(CTLR* xq)
{
  sim_debug(DBG_TRC, xq->dev, "xq_reset_santmr()\n");
  if (xq->var->sanity.enabled) {
    sim_debug(DBG_SAN, xq->dev, "SANITY TIMER RESETTING, qsecs: %d\n", xq->var->sanity.quarter_secs);

    /* reset sanity countdown timer to max count */
    xq->var->sanity.timer = xq->var->sanity.quarter_secs;
  }
}

t_stat xq_boot_host(CTLR* xq)
{
  sim_debug(DBG_TRC, xq->dev, "xq_boot_host()\n");
  /*
  The manual says the hardware should force the Qbus BDCOK low for
  3.6 microseconds, which will cause the host to reboot.

  Since the SIMH Qbus emulator does not have this functionality, we return
  a special STOP_ code, and let the CPU stop dispatch routine decide
  what the appropriate cpu-specific behavior should be.
  */
  return STOP_SANITY;
}

t_stat xq_system_id (CTLR* xq, const ETH_MAC dest, uint16 receipt_id)
{
  static smp_interlocked_uint32_var receipt = smp_var_init(0);
  ETH_PACK system_id;
  uint8* const msg = &system_id.msg[0];
  t_stat status;

  if (! smp_check_aligned(& receipt, FALSE))
    return SCPE_IERR;

  sim_debug(DBG_TRC, xq->dev, "xq_system_id()\n");

  /* reset system ID counter for next event */
  xq->var->idtmr = XQ_SYSTEM_ID_SECS * 4;

  if (xq->var->coalesce_latency) {
    /* Adjust latency ticks based on calibrated timer values */
    xq->var->coalesce_latency_ticks = (atomic_var(tmr_poll) * clk_tps * xq->var->coalesce_latency) / 1000000;
    }

  if (xq->var->type == XQ_T_DEQNA) /* DELQA-only function */
    return SCPE_NOFNC;  

  memset (&system_id, 0, sizeof(system_id));
  memcpy (&msg[0], dest, sizeof(ETH_MAC));
  memcpy (&msg[6], xq->var->setup.valid ? xq->var->setup.macs[0] : xq->var->mac, sizeof(ETH_MAC));
  msg[12] = 0x60;                         /* type */
  msg[13] = 0x02;                         /* type */
  msg[14] = 0x1C;                         /* character count */
  msg[15] = 0x00;                         /* character count */
  msg[16] = 0x07;                         /* code */
  msg[17] = 0x00;                         /* zero pad */
  if (receipt_id) {
    msg[18] = receipt_id & 0xFF;          /* receipt number */
    msg[19] = (receipt_id >> 8) & 0xFF;   /* receipt number */
  } else {
    uint32 xreceipt = smp_interlocked_increment_var(& receipt) - 1;
    msg[18] = xreceipt & 0xFF;            /* receipt number */
    msg[19] = (xreceipt >> 8) & 0xFF;     /* receipt number */
  }

                                          /* MOP VERSION */
  msg[20] = 0x01;                         /* type */
  msg[21] = 0x00;                         /* type */
  msg[22] = 0x03;                         /* length */
  msg[23] = 0x03;                         /* version */
  msg[24] = 0x01;                         /* eco */
  msg[25] = 0x00;                         /* user eco */

                                          /* FUNCTION */
  msg[26] = 0x02;                         /* type */
  msg[27] = 0x00;                         /* type */
  msg[28] = 0x02;                         /* length */
  msg[29] = 0x00;                         /* value 1 ??? */
  msg[30] = 0x00;                         /* value 2 */

                                          /* HARDWARE ADDRESS */
  msg[31] = 0x07;                         /* type */
  msg[32] = 0x00;                         /* type */
  msg[33] = 0x06;                         /* length */
  memcpy (&msg[34], xq->var->mac, sizeof(ETH_MAC)); /* ROM address */

                                          /* DEVICE TYPE */
  msg[40] = 37;                           /* type */
  msg[41] = 0x00;                         /* type */
  msg[42] = 0x01;                         /* length */
  msg[43] = 0x11;                         /* value (0x11=DELQA) */
  if (xq->var->type == XQ_T_DELQA_PLUS)   /* DELQA-T has different Device ID */
    msg[43] = 0x4B;                       /* value (0x4B(75)=DELQA-T) */

  /* write system id */
  system_id.len = 60;
  status = eth_write(xq->var->etherface, &system_id, NULL);

  if (DBG_PCK & xq->dev->dctrl)
    eth_packet_trace_ex(xq->var->etherface, system_id.msg, system_id.len, "xq-systemid", DBG_DAT & xq->dev->dctrl, DBG_PCK);

  return status;
}

/*
** service routine - used for ethernet reading loop
*/
t_stat xq_svc(RUN_SVC_DECL, UNIT* uptr)
{
  CTLR* xq = xq_unit2ctlr(uptr);

  AUTO_LOCK_NM(xq_autolock, *xq->xq_lock);
  RUN_SVC_CHECK_CANCELLED(uptr);

  return xq_svc_ex(RUN_PASS, uptr, xq);
}

t_stat xq_svc_ex(RUN_DECL, UNIT* uptr, CTLR* xq)
{
  /* if the receiver is enabled */
  if ((xq->var->mode == XQ_T_DELQA_PLUS) || (xq->var->csr & XQ_CSR_RE)) {
    t_stat status;

    /* First pump any queued packets into the system */
    if ((xq->var->ReadQ.count > 0) && ((xq->var->mode == XQ_T_DELQA_PLUS) || (~xq->var->csr & XQ_CSR_RL)))
      xq_process_rbdl(xq);

    /* Now read and queue packets that have arrived */
    /* This is repeated as long as they are available */
    do {
      /* read a packet from the ethernet - processing is via the callback */
      status = eth_read (xq->var->etherface, &xq->var->read_buffer, xq->var->rcallback);
    } while (status);

    /* Now pump any still queued packets into the system */
    if ((xq->var->ReadQ.count > 0) && ((xq->var->mode == XQ_T_DELQA_PLUS) || (~xq->var->csr & XQ_CSR_RL)))
      xq_process_rbdl(xq);
  }

  /* resubmit service timer */
  if (xq->var->must_poll || xq->var->poll && xq->var->mode != XQ_T_DELQA_PLUS)
    xq_activate(uptr, TRUE, xq->var->poll);

  return SCPE_OK;
}

/*
** service routine - used for timer based activities
*/
t_stat xq_tmrsvc(RUN_SVC_DECL, UNIT* uptr)
{
  CTLR* xq = xq_unit2ctlr(uptr);

  AUTO_LOCK_NM(xq_autolock, *xq->xq_lock);
  RUN_SVC_CHECK_CANCELLED(uptr);

  /* has sanity timer expired? if so, reboot */
  if (xq->var->sanity.enabled)
    if (--xq->var->sanity.timer <= 0)
      if (xq->var->mode != XQ_T_DELQA_PLUS)
        return xq_boot_host(xq);
      else { /* DELQA-T Host Inactivity Timer expiration means switch out of DELQA-T mode */
        sim_debug(DBG_TRC, xq->dev, "xq_tmrsvc(DELQA-PLUS Host Inactivity Expired\n");
        xq->var->mode = XQ_T_DELQA;
        xq->var->iba = xq->var->srr = 0;
        xq->var->var = XQ_VEC_MS | XQ_VEC_OS;
      }

  /* has system id timer expired? if so, do system id */
  if (--xq->var->idtmr <= 0) {
    const ETH_MAC mop_multicast = {0xAB, 0x00, 0x00, 0x02, 0x00, 0x00};
    xq_system_id(xq, mop_multicast, 0);
  }

  /* resubmit service timer */
  xq_activate(uptr, FALSE, 4);

  return SCPE_OK;
}


/* attach device: */
t_stat xq_attach(UNIT* uptr, char* cptr)
{
  t_stat status;
  char* tptr;
  CTLR* xq = xq_unit2ctlr(uptr);
  char buffer[80];                                          /* buffer for runtime input */

  sim_debug(DBG_TRC, xq->dev, "xq_attach(cptr=%s)\n", cptr);

  /* runtime selection of ethernet port? */
  if (*cptr == '?') {                                       /* I/O style derived from main() */
    memset (buffer, 0, sizeof(buffer));                     /* clear read buffer */
    eth_show (smp_stdout, uptr, 0, NULL);                   /* show ETH devices */
    smp_printf ("Select device (ethX or <device_name>)? "); /* prompt for device */
    cptr = read_line (buffer, sizeof(buffer), smp_stdin);   /* read command line */
    if (cptr == NULL) return SCPE_ARG;                      /* ignore EOF */
    if (*cptr == 0) return SCPE_ARG;                        /* ignore blank */
  }                                                         /* resume attaching */

  tptr = (char *) malloc(strlen(cptr) + 1);
  if (tptr == NULL) return SCPE_MEM;
  strcpy(tptr, cptr);

  xq->var->etherface = (ETH_DEV *) malloc(sizeof(ETH_DEV));
  if (!xq->var->etherface)
  {
    free(tptr);
    return SCPE_MEM;
  }

  status = eth_open(xq->var->etherface, cptr, xq->dev, DBG_ETH);
  if (status != SCPE_OK) {
    free(tptr);
    free(xq->var->etherface);
    xq->var->etherface = NULL;
    return status;
  }
  if (xq->var->poll == 0) {
    status = eth_set_async(xq->var->etherface, xq->var->coalesce_latency_ticks);
    if (status != SCPE_OK) {
      eth_close(xq->var->etherface);
      free(tptr);
      free(xq->var->etherface);
      xq->var->etherface = NULL;
      return status;
    }
    xq->var->must_poll = 0;
  } else {
    xq->var->must_poll = (SCPE_OK != eth_clr_async(xq->var->etherface));
  }
  if (SCPE_OK != eth_check_address_conflict (xq->var->etherface, &xq->var->mac)) {
    char buf[32];

    eth_mac_fmt(&xq->var->mac, buf);     /* format ethernet mac address */
    smp_printf("%s: MAC Address Conflict on LAN for address %s, change the MAC address to a unique value\n", xq->dev->name, buf);
    if (sim_log) fprintf (sim_log, "%s: MAC Address Conflict on LAN for address %s, change the MAC address to a unique value\n", xq->dev->name, buf);
    eth_close(xq->var->etherface);
    free(tptr);
    free(xq->var->etherface);
    xq->var->etherface = NULL;
    return SCPE_NOATT;
  }
  uptr->filename = tptr;
  uptr->flags |= UNIT_ATT;

  /* turn on transceiver power indicator */
  xq_csr_set_clr(xq, XQ_CSR_OK, 0);

  /* init read queue (first time only) */
  status = ethq_init(&xq->var->ReadQ, XQ_QUE_MAX);
  if (status != SCPE_OK) {
    eth_close(xq->var->etherface);
    free(tptr);
    free(xq->var->etherface);
    xq->var->etherface = NULL;
    return status;
  }

  if (xq->var->mode == XQ_T_DELQA_PLUS)
    eth_filter_hash (xq->var->etherface, 1, &xq->var->init.phys, 0, xq->var->init.mode & XQ_IN_MO_PRO, &xq->var->init.hash_filter);
  else
    if (xq->var->setup.valid) {
      int i, count = 0;
      ETH_MAC zeros = {0, 0, 0, 0, 0, 0};
      ETH_MAC filters[XQ_FILTER_MAX + 1];

      for (i = 0; i < XQ_FILTER_MAX; i++)
        if (memcmp(zeros, &xq->var->setup.macs[i], sizeof(ETH_MAC)))
          memcpy (filters[count++], xq->var->setup.macs[i], sizeof(ETH_MAC));
      eth_filter (xq->var->etherface, count, filters, xq->var->setup.multicast, xq->var->setup.promiscuous);
      }
    else
      /* reset the device with the new attach info */
      xq_reset(xq->dev);

  return SCPE_OK;
}

/* detach device: */

t_stat xq_detach(UNIT* uptr)
{
  CTLR* xq = xq_unit2ctlr(uptr);
  sim_debug(DBG_TRC, xq->dev, "xq_detach()\n");

  if (uptr->flags & UNIT_ATT) {
    eth_close (xq->var->etherface);
    free(xq->var->etherface);
    xq->var->etherface = NULL;
    free(uptr->filename);
    uptr->filename = NULL;
    uptr->flags &= ~UNIT_ATT;
    /* cancel service timers */
    sim_cancel(xq->unit[0]);
    sim_cancel(xq->unit[1]);
  }

  /* turn off transceiver power indicator */
  xq_csr_set_clr(xq, 0, XQ_CSR_OK);

  return SCPE_OK;
}

/*
 * Unfortunately there appears to be no lock-free way to consolidate per-controller interrupts
 * for multiple controllers into master interrupt. Such consolidation is possible to shared counter,
 * but it is impossible to atomically transfer this counter to VCPU interrupt state.
 *
 * Whereas non-atomic transfer would yield incorrect results.
 * Consider for example the sequence where controller A clears interrupt, then controller B raises it:
 *
 *           [controller A]                                  [controller B]
 *
 *     if (atomic_decr(interrupt_count) == 0)
 *              .
 *              .                                     if (atomic_incr(interrupt_count) == 1)
 *              .                                         SET_INT(XQ)
 *              .
 *              .
 *         CLR_INT(XQ)
 *
 * Because of race condition, interrupt incorrectly turns out cleared at the end.
 *
 * Therefore we have to set and clear interrupt under the protection of master lock.
 * We could have used separate lock for master lock, but it is better to use controller A's lock.
 *
 */

void xq_setint(CTLR* xq)
{
    if (xq->var->mode == XQ_T_DELQA_PLUS)
    {
        if (!xq->var->icr)
        {
            xq->var->pending_interrupt = 1;
            return;
        }
        xq->var->pending_interrupt = 0;
    }

    sim_debug(DBG_TRC, xq->dev, "xq_setint() - Generate Interrupt\n");

    if (xq->var->irq == 0)
    {
        xq->var->irq = 1;                         /* set ctrl int */

        if (xq->xq_lock != &xqa_lock)             /* acquire master lock unless already holding it */
            xqa_lock->lock();

        if (++smp_var(xq_pending_intrs) == 1)
            SET_INT (XQ);                         /* set master int */

        if (xq->xq_lock != &xqa_lock)             /* release master lock if was acquired */
            xqa_lock->unlock();
    }
}

void xq_clrint (CTLR* xq, t_bool intack)
{
    if (xq->var->irq == 1)
    {
        xq->var->irq = 0;                         /* clr ctrl int */

        if (xq->xq_lock != &xqa_lock)             /* acquire master lock unless already holding it */
            xqa_lock->lock();

        if (--smp_var(xq_pending_intrs) == 0)
            CLR_INT (XQ);                         /* clear master int */
        else
        {
            if (intack)
                SET_INT (XQ);                     /* set master int */
        }

        if (xq->xq_lock != &xqa_lock)             /* release master lock if was acquired */
            xqa_lock->unlock();
    }
}

int32 xq_int (void)
{
  int i;
  for (i = 0; i < XQ_MAX_CONTROLLERS; i++)
  {
    CTLR* xq = &xq_ctrl[i];
    
    if (xq->dev->flags & DEV_DIS)                 /* skip unconfigured devices */
        continue;

    (*xq->xq_lock)->lock();
    if (xq->var->irq) {                           /* if interrupt pending */
      xq_clrint(xq, TRUE);                        /* clear interrupt */
      (*xq->xq_lock)->unlock();
      return xq->dib->vec;                        /* return vector */
    }
    (*xq->xq_lock)->unlock();
  }
  return 0;                                       /* no interrupt request active */
}

void xq_csr_set_clr (CTLR* xq, uint16 set_bits, uint16 clear_bits)
{
  uint16 saved_csr = xq->var->csr;

  /* set the bits in the csr */
  xq->var->csr = (xq->var->csr | set_bits) & ~clear_bits;

  sim_debug_u16(DBG_CSR, xq->dev, xq_csr_bits, saved_csr, xq->var->csr, 1);

  /* check and correct the state of controller interrupt */

  /* if IE is transitioning, process it */
  if ((saved_csr ^ xq->var->csr) & XQ_CSR_IE) {

    /* if IE transitioning low and interrupt set, clear interrupt */
    if ((clear_bits & XQ_CSR_IE) && xq->var->irq)
      xq_clrint(xq);

    /* if IE transitioning high, and XI or RI is high,
       set interrupt if interrupt is off */
    if ((set_bits & XQ_CSR_IE) && (xq->var->csr & XQ_CSR_XIRI) && !xq->var->irq)
      xq_setint(xq);

  } else { /* IE is not transitioning */

    /* if interrupts are enabled */
    if (xq->var->csr & XQ_CSR_IE) {

      /* if XI or RI transitioning high and interrupt off, set interrupt */
      if (((saved_csr ^ xq->var->csr) & (set_bits & XQ_CSR_XIRI)) && !xq->var->irq) {
        xq_setint(xq);

      } else {

        /* if XI or RI transitioning low, and both XI and RI are now low,
           clear interrupt if interrupt is on */
        if (((saved_csr ^ xq->var->csr) & (clear_bits & XQ_CSR_XIRI))
         && !(xq->var->csr & XQ_CSR_XIRI)
         && xq->var->irq)
          xq_clrint(xq);
      }

    } /* IE enabled */

  } /* IE transitioning */
}
                        
static void xq_activate(UNIT* uptr, t_bool try_at_idletime, uint32 fraction)
{
    t_bool at_idletime = FALSE;

    /*
     * If idle sleep is used, avoid interrupting it mid-tick for activities that are 
     * not time critical and rather co-schedule the latter with next clock tick.
     */
    if (try_at_idletime)
        at_idletime = sim_vsmp_active ? sim_vsmp_idle_sleep : sim_idle_enab;

    if (at_idletime)
        sim_activate_clk_cosched(uptr, TMXR_MULT);
    else
        sim_activate(uptr, (weak_read_var(tmr_poll) * clk_tps) / fraction);
}

static void xq_activate_abs(UNIT* uptr, t_bool try_at_idletime, uint32 fraction)
{
    t_bool at_idletime = FALSE;

    /*
     * If idle sleep is used, avoid interrupting it mid-tick for activities that are 
     * not time critical and rather co-schedule the latter with next clock tick.
     */
    if (try_at_idletime)
        at_idletime = sim_vsmp_active ? sim_vsmp_idle_sleep : sim_idle_enab;

    if (at_idletime)
        sim_activate_clk_cosched_abs(uptr, TMXR_MULT);
    else
        sim_activate_abs(uptr, (weak_read_var(tmr_poll) * clk_tps) / fraction);
}

/*==============================================================================
/                               debugging routines
/=============================================================================*/


void xq_debug_setup(CTLR* xq)
{
  int   i;
  char  buffer[20];

  if (!(sim_deb && (xq->dev->dctrl & DBG_SET)))
    return;

  if (xq->var->write_buffer.msg[0])
    sim_debug(DBG_SET, xq->dev, "%s: setup> MOP info present!\n", xq->dev->name);

  for (i = 0; i < XQ_FILTER_MAX; i++) {
    eth_mac_fmt(&xq->var->setup.macs[i], buffer);
    sim_debug(DBG_SET, xq->dev, "%s: setup> set addr[%d]: %s\n", xq->dev->name, i, buffer);
  }

  if (xq->var->write_buffer.len > 128) {
    char buffer[20] = {0};
    uint16 len = xq->var->write_buffer.len;
    if (len & XQ_SETUP_MC) strcat(buffer, "MC ");
    if (len & XQ_SETUP_PM) strcat(buffer, "PM ");
    if (len & XQ_SETUP_LD) strcat(buffer, "LD ");
    if (len & XQ_SETUP_ST) strcat(buffer, "ST ");
    sim_debug(DBG_SET, xq->dev, "%s: setup> Length [%d =0x%X, LD:%d, ST:%d] info: %s\n",
      xq->dev->name, len, len, (len & XQ_SETUP_LD) >> 2, (len & XQ_SETUP_ST) >> 4, buffer);
  }
}

void xq_debug_turbo_setup(CTLR* xq)
{
  int i;
  char  buffer[64] = "";

  if (!(sim_deb && (xq->dev->dctrl & DBG_SET)))
    return;

  sim_debug(DBG_SET, xq->dev, "%s: setup> Turbo Initialization Block!\n", xq->dev->name);

  if (xq->var->init.mode & XQ_IN_MO_PRO) strcat(buffer, "PRO ");
  if (xq->var->init.mode & XQ_IN_MO_INT) strcat(buffer, "INT ");
  if (xq->var->init.mode & XQ_IN_MO_DRT) strcat(buffer, "DRC ");
  if (xq->var->init.mode & XQ_IN_MO_DTC) strcat(buffer, "DTC ");
  if (xq->var->init.mode & XQ_IN_MO_LOP) strcat(buffer, "LOP ");
  sim_debug(DBG_SET, xq->dev, "%s: setup> set Mode: %s\n", xq->dev->name, buffer);

  eth_mac_fmt(&xq->var->init.phys, buffer);
  sim_debug(DBG_SET, xq->dev, "%s: setup> set Physical MAC Address: %s\n", xq->dev->name, buffer);

  buffer[0] = '\0';
  for (i = 0; i < (int) sizeof(xq->var->init.hash_filter); i++) 
    sprintf(&buffer[strlen(buffer)], "%02X ", xq->var->init.hash_filter[i]);
  sim_debug(DBG_SET, xq->dev, "%s: setup> set Multicast Hash: %s\n", xq->dev->name, buffer);

  buffer[0] = '\0';
  if (xq->var->init.options & XQ_IN_OP_HIT) strcat(buffer, "HIT ");
  if (xq->var->init.options & XQ_IN_OP_INT) strcat(buffer, "INT ");
  sim_debug(DBG_SET, xq->dev, "%s: setup> set Options: %s\n", xq->dev->name, buffer);

  sim_debug(DBG_SET, xq->dev, "%s: setup> set Vector: %d =0x%X\n", 
            xq->dev->name, xq->var->init.vector, xq->var->init.vector);

  sim_debug(DBG_SET, xq->dev, "%s: setup> set Host Inactivity Timeout: %d seconds\n", 
            xq->dev->name, xq->var->init.hit_timeout);

  buffer[0] = '\0';
  for (i = 0; i < (int) sizeof(xq->var->init.bootpassword); i++) 
    sprintf(&buffer[strlen(buffer)], "%02X ", xq->var->init.bootpassword[i]);

  sim_debug(DBG_SET, xq->dev, "%s: setup> set Boot Password: %s\n", xq->dev->name, buffer);

  sim_debug(DBG_SET, xq->dev, "%s: setup> set Receive Ring Buffer Address:  %02X%04X\n", 
            xq->dev->name, xq->var->init.rdra_h, xq->var->init.rdra_l);
  sim_debug(DBG_SET, xq->dev, "%s: setup> set Transmit Ring Buffer Address: %02X%04X\n", 
            xq->dev->name, xq->var->init.tdra_h, xq->var->init.tdra_l);
}
