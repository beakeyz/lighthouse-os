#ifndef __APIC__
#define __APIC__

#define APIC_EOI 0xB0
#define LAPIC_ID 0x20
#define APIC_ICR1 0x300
#define APIC_ICR2 0x310
#define APIC_LVT_TIMER 0x320
#define APIC_LINT1 0x350
#define APIC_LINT2 0x360

// TODO: look for the address thats (should be) mapped to memory
// TODO: parse MADT
// TODO: for all this, we'll need to finish the mm first ;-;



#endif // !__APIC__
