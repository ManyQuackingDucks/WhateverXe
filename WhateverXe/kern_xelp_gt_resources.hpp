// Shared Gen12 small-GT resource contract, independent of product identity.
#ifndef KERN_XELP_GT_RESOURCES_HPP
#define KERN_XELP_GT_RESOURCES_HPP
#include <stdint.h>
namespace XELP {
struct GTResources {
    uint32_t slices {}, dss {}, nativeSubslices {}, eus {}, l3Banks {}, urbKiB {};
};
using SmallGTResources = GTResources;

// Gen12 SSEU: one slice, up to two DSS in these small GT SKUs, one disable
// bit per EU pair. Native subslices are twice the DSS population. Preserve
// the physical LP backend; capacity is independent of the PCI product label.
// Four L3 banks/256KiB is the supported small-GT capacity, not eus*constant.
inline bool smallGTResources(uint32_t slice, uint32_t dss, uint32_t euDisable,
                      uint32_t l3Disable, SmallGTResources &out) {
    out = {};
    if (slice != 1 || !dss || (dss&~0x3FU) || (euDisable&~0xFFU) || l3Disable != 0xF0)
        return false;
    const auto count=static_cast<uint32_t>(__builtin_popcount(dss));
    const auto eus=2*count*(8-static_cast<uint32_t>(__builtin_popcount(euDisable)));
    if (count>2 || (eus!=16 && eus!=24 && eus!=32)) return false;
    out = {1, count, 2*count, eus, 4, 256};
    return true;
}

// Gen12 media-engine fuse window. The complement-encoded low byte carries the
// primary VCS0 pair (bit0) and the optional VCS2 (bit2); bits 16..19 carry the
// enhance window, which must be present. Small-GT donors require the primary
// VCS0 pair, but fused parts that populate only the secondary VCS2 slot report
// the same supported resource envelope and decode through this pattern.
inline bool videoEngine(uint32_t fuses) {
    const auto video=(~fuses)&0xFF,enhance=((~fuses)>>16)&0xF;
    return enhance==1 && (video==1 || video==4 || video==5);
}
// TGL GT1 and ADL/RPL-S use the same Gen12 fuse fields, qualified through the
// shared media-engine window above. Product admission is the caller's policy.
inline bool decodeSmallGT(const uint32_t *fuses, SmallGTResources &out) {
    out={};
    if(!fuses)return false;
    for(unsigned i=0;i<5;++i)if(fuses[i]==UINT32_MAX)return false;
    if(!videoEngine(fuses[4]))return false;
    return smallGTResources(fuses[0]&0xFF,fuses[1]&0x3F,
                           fuses[2]&0xFF,fuses[3]&0xFF,out);
}

// ADL-P Gen12.3 retains the Xe-LP fuse encoding, with up to six DSS and
// eight L3 banks. Capacity comes from fuses rather than a CPU/board model.
// The donor URB backend handles 4/6/8 banks; reject unsupported capacities.
inline bool decodeADLPGT(const uint32_t *fuses, GTResources &out) {
    out={};
    if(!fuses)return false;
    for(unsigned i=0;i<5;++i)if(fuses[i]==UINT32_MAX)return false;
    const auto slice=fuses[0]&0xFF,mask=fuses[1]&0x3F;
    if(slice!=1 || !mask || !videoEngine(fuses[4]))return false;
    const auto dss=static_cast<uint32_t>(__builtin_popcount(mask));
    const auto perDSS=2*(8-static_cast<uint32_t>(__builtin_popcount(fuses[2]&0xFF)));
    const auto banks=8-static_cast<uint32_t>(__builtin_popcount(fuses[3]&0xFF));
    if((perDSS!=8 && perDSS!=12 && perDSS!=16) || (banks!=4 && banks!=6 && banks!=8))return false;
    out={1,dss,2*dss,dss*perDSS,banks,64*banks};
    return true;
}

// TGL U/Y uses the same fuse fields and donor resource backend as ADL-P.
// Qualify each capacity independently: EU count does not determine L3 banks.
inline bool decodeTGLGT2(const uint32_t *fuses, GTResources &out) {
    return decodeADLPGT(fuses,out);
}

}
#endif
