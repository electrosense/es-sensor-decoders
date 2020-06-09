
SUBDIRS := acarsdec/. dump1090/. rtl-ais_es/. LTE-Cell-Scanner/.

# Limited version
#.PHONY : all $(SUBDIRS)
#	all : $(SUBDIRS)
#
#$(SUBDIRS) :
#	$(MAKE) -C $@ clean all

#Arbitrary target


#SUBDIRS_POST := $(foreach t,$(SUBDIRS),$(addsuffix "/.",$t))
TARGETS := all clean  # whatever else, but must not contain '/'
# foo/.all bar/.all foo/.clean bar/.clean
SUBDIRS_TARGETS := \
	$(foreach t,$(TARGETS),$(addsuffix $t,$(SUBDIRS)))

.PHONY : $(TARGETS) $(SUBDIRS_TARGETS)

# static pattern rule, expands into:
#all clean : % : foo/.% bar/.%

$(TARGETS) : % : $(addsuffix %,$(SUBDIRS))
	@echo 'Done "$*" target'

#     # here, for foo/.all:
#   $(@D) is foo
#   #   $(@F) is .all, with leading period
#   $(@F:.%=%) is just all
#
#   -j 2 should not be increased in RPi2 (memory exhausts easily)
$(SUBDIRS_TARGETS) :
	$(MAKE) -C $(@D) $(@F:.%=%) -j 2

