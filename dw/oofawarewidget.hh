#ifndef __DW_OOFAWAREWIDGET_HH__
#define __DW_OOFAWAREWIDGET_HH__

#include "core.hh"
#include "outofflowmgr.hh"

namespace dw {

namespace oof {

/**
 * \brief Base class for widgets which can act as container and
 *     generator for widgets out of flow.
 *
 * (Perhaps it should be diffenciated between the two roles, container
 * and generator, but this would make multiple inheritance necessary.)
 */
class OOFAwareWidget: public core::Widget
{
protected:
   enum { OOFM_FLOATS, OOFM_ABSOLUTE, OOFM_FIXED, NUM_OOFM };
   enum { PARENT_REF_OOFM_BITS = 2,
          PARENT_REF_OOFM_MASK = (1 << PARENT_REF_OOFM_BITS) - 1 };

public:
   OOFAwareWidget *oofContainer[NUM_OOFM];
   oof::OutOfFlowMgr *outOfFlowMgr[NUM_OOFM];

protected:
   void initOutOfFlowMgrs ();
   void sizeAllocateStart (core::Allocation *allocation);
   void sizeAllocateEnd ();

   inline OutOfFlowMgr *searchOutOfFlowMgr (int oofmIndex)
   { return oofContainer[oofmIndex] ?
         oofContainer[oofmIndex]->outOfFlowMgr[oofmIndex] : NULL; }
public:
   OOFAwareWidget ();
   ~OOFAwareWidget ();

   virtual void borderChanged (int y, core::Widget *vloat);
   virtual void oofSizeChanged (bool extremesChanged);
   virtual int getLineBreakWidth (); // Should perhaps be renamed.
};

} // namespace oof

} // namespace dw

#endif // __DW_OOFAWAREWIDGET_HH__