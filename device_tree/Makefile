KDIR ?= /lib/modules/`uname -r`/build

DTC = dtc
DTCFLAGS = -b 0
dtc_cpp_flags  = -x assembler-with-cpp -nostdinc         \
                 -I $(KDIR)/arch/arm/boot/dts \
                 -I $(KDIR)/include/ \
                 -undef -D__DTS__

DTCINCLUDES = -i $(KDIR)/arch/arm/boot/dts

%.dtb: %.dts
	cpp $(dtc_cpp_flags) < $< | $(DTC) $(DTCINCLUDES) -I dts -O dtb $(DTCFLAGS) -o $@ -

all: isocfpga_cyclone5_DE1-SoC.dtb
