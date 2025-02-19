/*
 * Dillo Widget
 *
 * Copyright 2005-2007, 2014 Sebastian Geerken <sgeerken@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

//#define DBG

#include "table.hh"
#include "../lout/msg.h"
#include "../lout/misc.hh"
#include "../lout/debug.hh"

using namespace lout;

namespace dw {

bool Table::adjustTableMinWidth = true;
int Table::CLASS_ID = -1;

Table::Table(bool limitTextWidth)
{
   DBG_OBJ_CREATE ("dw::Table");
   registerName ("dw::Table", &CLASS_ID);
   setButtonSensitive(false);

   this->limitTextWidth = limitTextWidth;

   rowClosed = false;

   numRows = 0;
   numCols = 0;
   curRow = -1;
   curCol = 0;

   DBG_OBJ_SET_NUM ("numCols", numCols);
   DBG_OBJ_SET_NUM ("numRows", numCols);

   children = new misc::SimpleVector <Child*> (16);
   colExtremes = new misc::SimpleVector<core::Extremes> (8);
   colWidthSpecified = new misc::SimpleVector<bool> (8);
   colWidthPercentage = new misc::SimpleVector<bool> (8);
   colWidths = new misc::SimpleVector <int> (8);
   cumHeight = new misc::SimpleVector <int> (8);
   rowSpanCells = new misc::SimpleVector <int> (8);
   baseline = new misc::SimpleVector <int> (8);
   rowStyle = new misc::SimpleVector <core::style::Style*> (8);

   colWidthsUpToDateWidthColExtremes = true;
   DBG_OBJ_SET_BOOL ("colWidthsUpToDateWidthColExtremes",
                     colWidthsUpToDateWidthColExtremes);

   numColWidthSpecified = 0;
   numColWidthPercentage = 0;

   redrawX = 0;
   redrawY = 0;
}

Table::~Table()
{
   for (int i = 0; i < children->size (); i++) {
      if (children->get(i)) {
         switch (children->get(i)->type) {
         case Child::CELL:
            delete children->get(i)->cell.widget;
            break;
         case Child::SPAN_SPACE:
            break;
         }

         delete children->get(i);
      }
   }

   for (int i = 0; i < rowStyle->size (); i++)
      if (rowStyle->get (i))
         rowStyle->get(i)->unref ();

   delete children;
   delete colExtremes;
   delete colWidthSpecified;
   delete colWidthPercentage;
   delete colWidths;
   delete cumHeight;
   delete rowSpanCells;
   delete baseline;
   delete rowStyle;

   DBG_OBJ_DELETE ();
}

void Table::sizeRequestSimpl (core::Requisition *requisition)
{
   DBG_OBJ_ENTER0 ("resize", 0, "sizeRequestImpl");

   forceCalcCellSizes (true);

   /**
    * \bug Baselines are not regarded here.
    */
   requisition->width =
      boxDiffWidth () + (numCols + 1) * getStyle()->hBorderSpacing;
   for (int col = 0; col < numCols; col++)
      requisition->width += colWidths->get (col);

   requisition->ascent =
      boxDiffHeight () + cumHeight->get (numRows) + getStyle()->vBorderSpacing;
   requisition->descent = 0;

   correctRequisition (requisition, core::splitHeightPreserveDescent, true,
                       false);

   // For the order, see similar reasoning for dw::Textblock.
   correctRequisitionByOOF (requisition, core::splitHeightPreserveDescent);

   DBG_OBJ_LEAVE ();
}

void Table::getExtremesSimpl (core::Extremes *extremes)
{
   DBG_OBJ_ENTER0 ("resize", 0, "getExtremesImpl");

   if (numCols == 0)
      extremes->minWidth = extremes->minWidthIntrinsic = extremes->maxWidth =
         extremes->maxWidthIntrinsic = extremes->adjustmentWidth =
         boxDiffWidth ();
   else {
      forceCalcColumnExtremes ();

      extremes->minWidth = extremes->minWidthIntrinsic = extremes->maxWidth =
         extremes->maxWidthIntrinsic = extremes->adjustmentWidth =
         (numCols + 1) * getStyle()->hBorderSpacing + boxDiffWidth ();
      for (int col = 0; col < numCols; col++) {
         extremes->minWidth += colExtremes->getRef(col)->minWidth;
         extremes->minWidthIntrinsic +=
            colExtremes->getRef(col)->minWidthIntrinsic;
         extremes->maxWidth += colExtremes->getRef(col)->maxWidth;
         extremes->maxWidthIntrinsic +=
            colExtremes->getRef(col)->maxWidthIntrinsic;
         extremes->adjustmentWidth += colExtremes->getRef(col)->adjustmentWidth;
      }
   }

   correctExtremes (extremes, true);

   // For the order, see similar reasoning for dw::Textblock.
   correctExtremesByOOF (extremes);

   DBG_OBJ_LEAVE ();
}

void Table::sizeAllocateImpl (core::Allocation *allocation)
{
   DBG_OBJ_ENTER ("resize", 0, "sizeAllocateImpl", "%d, %d; %d * (%d + %d)",
                  allocation->x, allocation->y, allocation->width,
                  allocation->ascent, allocation->descent);

   sizeAllocateStart (allocation);

   calcCellSizes (true);

   /**
    * \bug Baselines are not regarded here.
    */

   int offy = allocation->y + boxOffsetY () + getStyle()->vBorderSpacing;
   int x = allocation->x + boxOffsetX () + getStyle()->hBorderSpacing;

   for (int col = 0; col < numCols; col++) {
      for (int row = 0; row < numRows; row++) {
         int n = row * numCols + col;
         if (childDefined (n)) {
            int width = (children->get(n)->cell.colspanEff - 1)
               * getStyle()->hBorderSpacing;
            for (int i = 0; i < children->get(n)->cell.colspanEff; i++)
               width += colWidths->get (col + i);

            core::Allocation childAllocation;
            core::Requisition childRequisition;

            children->get(n)->cell.widget->sizeRequest (&childRequisition);

            childAllocation.x = x;
            childAllocation.y = cumHeight->get (row) + offy;
            childAllocation.width = width;
            childAllocation.ascent = childRequisition.ascent;
            childAllocation.descent =
               cumHeight->get (row + children->get(n)->cell.rowspan)
               - cumHeight->get (row) - getStyle()->vBorderSpacing
               - childRequisition.ascent;
            children->get(n)->cell.widget->sizeAllocate (&childAllocation);
         }
      }

      x += colWidths->get (col) + getStyle()->hBorderSpacing;
   }

   sizeAllocateEnd ();

   DBG_OBJ_LEAVE ();
}

void Table::resizeDrawImpl ()
{
   queueDrawArea (redrawX, 0, allocation.width - redrawX, getHeight ());
   queueDrawArea (0, redrawY, allocation.width, getHeight () - redrawY);
   redrawX = allocation.width;
   redrawY = getHeight ();
}

int Table::getAvailWidthOfChild (Widget *child, bool forceValue)
{
   DBG_OBJ_ENTER ("resize", 0, "getAvailWidthOfChild", "%p, %s",
                  child, forceValue ? "true" : "false");

   int width;
   oof::OutOfFlowMgr *oofm;

   if (isWidgetOOF(child) && (oofm = getWidgetOutOfFlowMgr(child)) &&
       oofm->dealingWithSizeOfChild (child))
      width = oofm->getAvailWidthOfChild (child, forceValue);
   else {
      // We do not calculate the column widths at this point, because
      // this tends to be rather inefficient for tables with many
      // cells:
      //
      // For each of the n cells, some text is added (say, only one word
      // per cell). Textblock::addText will eventually (via addText0
      // etc.) call this method, Table::getAvailWidthOfChild. If
      // calcCellSizes() is called here, this will call
      // forceCalcCellSizes(), since the last call, sizes have to be
      // re-calculated (because cells have been added). This will
      // calculate the extremes for each existing cell, so
      // Widget::getExtremes is called n * (n + 1) / 2 times. Even if the
      // extremes are cached (so that getExtremesImpl does not have to be
      // called in each case), this would make rendering tables with more
      // than a few hundred cells unacceptably slow.
      //
      // Instead, column widths are calculated in Table::sizeRequestImpl.
      //
      // An alternative would be incremental resizing for tables; this
      // approach resembles the behaviour before GROWS.

      // TODO Does it still make sense to return -1 when forceValue is
      // set?
      if (forceValue)
         width = calcAvailWidthForDescendant (child);
      else
         width = -1;
   }

   DBG_OBJ_MSGF ("resize", 1, "=> %d", width);
   DBG_OBJ_LEAVE ();
   return width;
}

int Table::calcAvailWidthForDescendant (Widget *child)
{
   DBG_OBJ_ENTER ("resize", 0, "calcAvailWidthForDescendant", "%p", child);

   // "child" is not a direct child, but a direct descendant. Search
   // for the actual children.
   Widget *actualChild = child;
   while (actualChild != NULL && actualChild->getParent () != this)
      actualChild = actualChild->getParent ();

   assert (actualChild != NULL);

   // ActualChild->parentRef contains (indirectly) the position in the
   // children array (see addCell()), so the column can be easily
   // determined.
   int childNo = getParentRefInFlowSubRef (actualChild->parentRef);
   int col = childNo % numCols;
   DBG_OBJ_MSGF ("resize", 1, "actualChild = %p, "
                 "childNo = getParentRefInFlowSubRef (%d) = %d, "
                 "column = %d %% %d = %d",
                 actualChild, actualChild->parentRef, childNo, childNo,
                 numCols, col);
   int colspanEff = children->get(childNo)->cell.colspanEff;
   DBG_OBJ_MSGF ("resize", 1, "calculated from column %d, colspanEff = %d",
                 col, colspanEff);

   int width = (colspanEff - 1) * getStyle()->hBorderSpacing;
   for (int i = 0; i < colspanEff; i++)
      width += colWidths->get (col + i);
   width = misc::max (width, 0);

   if (child != actualChild) {
      // For table cells (direct children: child == actualChild), CSS
      // 'width' is already regarded in the column calculation.
      // However, for children of the table cells, CSS 'width' must be
      // regarded here.

      int corrWidth = width;
      child->calcFinalWidth (child->getStyle(), -1, this, 0, true, &corrWidth);
      
      // But better not exceed it ... (TODO: Only here?)
      width = misc::min (width, corrWidth);
   }

   DBG_OBJ_MSGF ("resize", 1, "=> %d", width);
   DBG_OBJ_LEAVE ();
   return width;
}

int Table::applyPerWidth (int containerWidth, core::style::Length perWidth)
{
   return core::style::multiplyWithPerLength (containerWidth, perWidth);
}

int Table::applyPerHeight (int containerHeight, core::style::Length perHeight)
{
   return core::style::multiplyWithPerLength (containerHeight, perHeight);
}

void Table::containerSizeChangedForChildren ()
{
   DBG_OBJ_ENTER0 ("resize", 0, "containerSizeChangedForChildren");

   for (int col = 0; col < numCols; col++) {
      for (int row = 0; row < numRows; row++) {
         int n = row * numCols + col;
         if (childDefined (n))
            children->get(n)->cell.widget->containerSizeChanged ();
      }
   }

   containerSizeChangedForChildrenOOF ();

   DBG_OBJ_LEAVE ();
}

bool Table::affectsSizeChangeContainerChild (core::Widget *child)
{
   DBG_OBJ_ENTER ("resize", 0, "affectsSizeChangeContainerChild", "%p", child);

   bool ret;

   // This is a bit more complicated, as compared to the standard
   // implementation (Widget::affectsSizeChangeContainerChild).
   // Height would handled the same way, but width is more
   // complicated: we would have to track numerous values here. Always
   // returning true is correct in all cases, but generally
   // inefficient.

   // TODO Better solution?

   ret = true;

   DBG_OBJ_MSGF ("resize", 1, "=> %s", ret ? "true" : "false");
   DBG_OBJ_LEAVE ();
   return ret;
}

bool Table::usesAvailWidth ()
{
   return true;
}

bool Table::isBlockLevel ()
{
   return true;
}

void Table::drawLevel (core::View *view, core::Rectangle *area, int level,
                       core::DrawingContext *context)
{
   DBG_OBJ_ENTER ("draw", 0, "Table::drawLevel", "[%d, %d, %d * %d], %s",
                  area->x, area->y, area->width, area->height,
                  stackingLevelText (level));

#if 0
   // This old code belongs perhaps to the background. Check when reactivated.
   int offx = getStyle()->boxOffsetX () + getStyle()->hBorderSpacing;
   int offy = getStyle()->boxOffsetY () + getStyle()->vBorderSpacing;
   int width = getContentWidth ();
   
   // This part seems unnecessary. It also segfaulted sometimes when
      // cumHeight size was less than numRows. --jcid
   for (int row = 0; row < numRows; row++) {
      if (rowStyle->get (row))
         drawBox (view, rowStyle->get (row), area,
                  offx, offy + cumHeight->get (row),
                  width - 2*getStyle()->hBorderSpacing,
                  cumHeight->get (row + 1) - cumHeight->get (row)
                  - getStyle()->vBorderSpacing, false);
   }
#endif

   switch (level) {
   case SL_IN_FLOW:
      for (int i = 0; i < children->size (); i++) {
         if (childDefined (i)) {
            Widget *child = children->get(i)->cell.widget;
            core::Rectangle childArea;
            if (!core::StackingContextMgr::handledByStackingContextMgr (child)
                && child->intersects (this, area, &childArea))
               child->draw (view, &childArea, context);
         }
      }
      break;

   default:
      OOFAwareWidget::drawLevel (view, area, level, context);
      break;
   }

   DBG_OBJ_LEAVE ();
}

core::Widget *Table::getWidgetAtPointLevel (int x, int y, int level,
                                            core::GettingWidgetAtPointContext
                                            *context)
{
   DBG_OBJ_ENTER ("events", 0, "Table::getWidgetAtPointLevel", "%d, %d, %s",
                  x, y, stackingLevelText (level));

   Widget *widgetAtPoint = NULL;

   switch (level) {
   case SL_IN_FLOW:
      for (int i = children->size () - 1; widgetAtPoint == NULL && i >= 0;
           i--) {
         if (childDefined (i)) {
            Widget *child = children->get(i)->cell.widget;
            if (!core::StackingContextMgr::handledByStackingContextMgr (child))
               widgetAtPoint = child->getWidgetAtPoint (x, y, context);
         }
      }
      break;

   default:
      widgetAtPoint =
         OOFAwareWidget::getWidgetAtPointLevel (x, y, level, context);
      break;
   }

   DBG_OBJ_MSGF ("events", 1, "=> %p", widgetAtPoint);
   DBG_OBJ_LEAVE ();

   return widgetAtPoint;
}

void Table::removeChild (Widget *child)
{
   /** \bug Not implemented. */
}

core::Iterator *Table::iterator (core::Content::Type mask, bool atEnd)
{
   return new TableIterator (this, mask, atEnd);
}

void Table::addCell (Widget *widget, int colspan, int rowspan)
{
   DBG_OBJ_ENTER ("resize", 0, "addCell", "%p, %d, %d",
                  widget, colspan, rowspan);

   const int maxspan = 100;
   Child *child;
   int colspanEff;

   // We limit the values for colspan and rowspan to avoid
   // attacks by malicious web pages.
   if (colspan > maxspan || colspan < 0) {
      MSG_WARN("colspan = %d is set to %d.\n", colspan, maxspan);
      colspan = maxspan;
   }
   if (rowspan > maxspan || rowspan <= 0) {
      MSG_WARN("rowspan = %d is set to %d.\n", rowspan, maxspan);
      rowspan = maxspan;
   }

   if (numRows == 0) {
      // to prevent a crash
      MSG("addCell: cell without row.\n");
      addRow (NULL);
   }

   if (rowClosed) {
      MSG_WARN("Last cell had colspan=0.\n");
      addRow (NULL);
   }

   if (colspan == 0) {
      colspanEff = misc::max (numCols - curCol, 1);
      rowClosed = true;
   } else
      colspanEff = colspan;

   // Find next free cell-
   while (curCol < numCols &&
          (child = children->get(curRow * numCols + curCol)) != NULL &&
          child->type == Child::SPAN_SPACE)
      curCol++;

   _MSG("Table::addCell numCols=%d,curCol=%d,colspan=%d,colspanEff=%d\n",
       numCols, curCol, colspan, colspanEff);

   // Increase children array, when necessary.
   if (curRow + rowspan > numRows)
      reallocChildren (numCols, curRow + rowspan);
   if (curCol + colspanEff > numCols)
      reallocChildren (curCol + colspanEff, numRows);

   // Fill span space.
   for (int col = 0; col < colspanEff; col++)
      for (int row = 0; row < rowspan; row++)
         if (!(col == 0 && row == 0)) {
            int i = (curRow + row) * numCols + curCol + col;

            child = children->get(i);
            if (child) {
               MSG("Overlapping spans in table.\n");
               assert(child->type == Child::SPAN_SPACE);
               delete child;
            }
            child = new Child ();
            child->type = Child::SPAN_SPACE;
            child->spanSpace.startCol = curCol;
            child->spanSpace.startRow = curRow;
            children->set (i, child);
         }

   // Set the "root" cell.
   child = new Child ();
   child->type = Child::CELL;
   child->cell.widget = widget;
   child->cell.colspanOrig = colspan;
   child->cell.colspanEff = colspanEff;
   child->cell.rowspan = rowspan;
   children->set (curRow * numCols + curCol, child);

   // The position in the children array is (indirectly) assigned to parentRef,
   // although incremental resizing is not implemented. Useful, e. g., in
   // calcAvailWidthForDescendant(). See also reallocChildren().
   widget->parentRef = makeParentRefInFlow (curRow * numCols + curCol);
   DBG_OBJ_SET_NUM_O (widget, "parentRef", widget->parentRef);

   curCol += colspanEff;
   
   widget->setParent (this);
   if (rowStyle->get (curRow))
      widget->setBgColor (rowStyle->get(curRow)->backgroundColor);
   queueResize (0, true);

#if 0
   // show table structure in stdout
   for (int row = 0; row < numRows; row++) {
      for (int col = 0; col < numCols; col++) {
         int n = row * numCols + col;
         if (!(child = children->get (n))) {
            MSG("[null     ] ");
         } else if (children->get(n)->type == Child::CELL) {
            MSG("[CELL rs=%d] ", child->cell.rowspan);
         } else if (children->get(n)->type == Child::SPAN_SPACE) {
            MSG("[SPAN rs=%d] ", child->cell.rowspan);
         } else {
            MSG("[Unk.     ] ");
         }
      }
      MSG("\n");
   }
   MSG("\n");
#endif

   DBG_OBJ_LEAVE ();
}

void Table::addRow (core::style::Style *style)
{
   curRow++;

   if (curRow >= numRows)
      reallocChildren (numCols, curRow + 1);

   if (rowStyle->get (curRow))
      rowStyle->get(curRow)->unref ();

   rowStyle->set (curRow, style);
   if (style)
      style->ref ();

   curCol = 0;
   rowClosed = false;
}

AlignedTableCell *Table::getCellRef ()
{
   core::Widget *child;

   for (int row = 0; row <= numRows; row++) {
      int n = curCol + row * numCols;
      if (childDefined (n)) {
         child = children->get(n)->cell.widget;
         if (child->instanceOf (AlignedTableCell::CLASS_ID))
            return (AlignedTableCell*)child;
      }
   }

   return NULL;
}

const char *Table::getExtrModName (ExtrMod mod)
{
   switch (mod) {
   case MIN:
      return "MIN";

   case MIN_INTR:
      return "MIN_INTR";

   case MIN_MIN:
      return "MIN_MIN";

   case MAX_MIN:
      return "MAX_MIN";

   case MAX:
      return "MAX";

   case MAX_INTR:
      return "MAX_INTR";

   case DATA:
      return "DATA";

   default:
      misc::assertNotReached ();
      return NULL;
   }
}

int Table::getExtreme (core::Extremes *extremes, ExtrMod mod)
{
   switch (mod) {
   case MIN:
      return extremes->minWidth;

   case MIN_INTR:
      return extremes->minWidthIntrinsic;

   case MIN_MIN:
      return misc::min (extremes->minWidth, extremes->minWidthIntrinsic);

   case MAX_MIN:
      return misc::max (extremes->minWidth, extremes->minWidthIntrinsic);

   case MAX:
      return extremes->maxWidth;

   case MAX_INTR:
      return extremes->maxWidthIntrinsic;

   default:
      misc::assertNotReached ();
      return 0;
   }
}

void Table::setExtreme (core::Extremes *extremes, ExtrMod mod, int value)
{
   switch (mod) {
   case MIN:
      extremes->minWidth = value;
      break;

   case MIN_INTR:
      extremes->minWidthIntrinsic = value;
      break;

   // MIN_MIN and MAX_MIN not supported here.

   case MAX:
      extremes->maxWidth = value;
      break;

   case MAX_INTR:
      extremes->maxWidthIntrinsic = value;
      break;

   default:
      misc::assertNotReached ();
   }
}

int Table::getColExtreme (int col, ExtrMod mod, void *data)
{
   switch (mod) {
   case DATA:
      return ((misc::SimpleVector<int>*)data)->get (col);

   default:
      return getExtreme (colExtremes->getRef(col), mod);
   }
}

void Table::setColExtreme (int col, ExtrMod mod, void *data, int value)
{
   switch (mod) {
   case DATA:
      ((misc::SimpleVector<int>*)data)->set (col, value);
      /* fallthrough */

   default:
      setExtreme (colExtremes->getRef(col), mod, value);
   }
}

void Table::reallocChildren (int newNumCols, int newNumRows)
{
   assert (newNumCols >= numCols);
   assert (newNumRows >= numRows);

   children->setSize (newNumCols * newNumRows);

   if (newNumCols > numCols) {
      // Complicated case, array got also wider.
      for (int row = newNumRows - 1; row >= 0; row--) {
         int colspan0Col = -1, colspan0Row = -1;

         // Copy old part.
         for (int col = numCols - 1; col >= 0; col--) {
            int n = row * newNumCols + col;
            children->set (n, children->get (row * numCols + col));
            if (children->get (n)) {
               switch (children->get(n)->type) {
               case Child::CELL:
                  if (children->get(n)->cell.colspanOrig == 0) {
                     colspan0Col = col;
                     colspan0Row = row;
                     children->get(n)->cell.colspanEff = newNumCols - col;
                  }
                  break;
               case Child::SPAN_SPACE:
                  if (children->get(children->get(n)->spanSpace.startRow
                                    * numCols +
                                    children->get(n)->spanSpace.startCol)
                      ->cell.colspanOrig == 0) {
                     colspan0Col = children->get(n)->spanSpace.startCol;
                     colspan0Row = children->get(n)->spanSpace.startRow;
                  }
                  break;
               }
            }
         }

         // Fill rest of the column.
         if (colspan0Col == -1) {
            for (int col = numCols; col < newNumCols; col++)
               children->set (row * newNumCols + col, NULL);
         } else {
            for (int col = numCols; col < newNumCols; col++) {
               Child *child = new Child ();
               child->type = Child::SPAN_SPACE;
               child->spanSpace.startCol = colspan0Col;
               child->spanSpace.startRow = colspan0Row;
               children->set (row * newNumCols + col, child);
            }
         }
      }
   }

   // Bottom part of the children array.
   for (int row = numRows; row < newNumRows; row++)
      for (int col = 0; col < newNumCols; col++)
         children->set (row * newNumCols + col, NULL);

   // Simple arrays.
   rowStyle->setSize (newNumRows);
   for (int row = numRows; row < newNumRows; row++)
      rowStyle->set (row, NULL);
   // Rest is increased, when needed.

   if (newNumCols > numCols) {
      // Re-calculate parentRef. See addCell().
      for (int row = 1; row < newNumRows; row++)
         for (int col = 0; col < newNumCols; col++) {
            int n = row * newNumCols + col;
            Child *child = children->get (n);
            if (child != NULL && child->type == Child::CELL) {
               child->cell.widget->parentRef = makeParentRefInFlow (n);
               DBG_OBJ_SET_NUM_O (child->cell.widget, "parentRef",
                                  child->cell.widget->parentRef);
            }
         }
   }

   numCols = newNumCols;
   numRows = newNumRows;

   // We initiate the column widths with a random value, to have a
   // defined available width for the children before the column
   // widths are actually calculated.

   colWidths->setSize (numCols, 100);

   DBG_IF_RTFL {
      DBG_OBJ_SET_NUM ("colWidths.size", colWidths->size ());
      for (int i = 0; i < colWidths->size (); i++)
         DBG_OBJ_ARRSET_NUM ("colWidths", i, colWidths->get (i));
   }

   DBG_OBJ_SET_NUM ("numCols", numCols);
   DBG_OBJ_SET_NUM ("numRows", numCols);
}

// ----------------------------------------------------------------------

void Table::calcCellSizes (bool calcHeights)
{
   DBG_OBJ_ENTER ("resize", 0, "calcCellSizes", "%s",
                  calcHeights ? "true" : "false");

   bool sizeChanged = needsResize () || resizeQueued ();
   bool extremesChanget = extremesChanged () || extremesQueued ();

   if (calcHeights ? (extremesChanget || sizeChanged) :
       (extremesChanget || !colWidthsUpToDateWidthColExtremes))
      forceCalcCellSizes (calcHeights);

   DBG_OBJ_LEAVE ();
}


void Table::forceCalcCellSizes (bool calcHeights)
{
   DBG_OBJ_ENTER ("resize", 0, "forceCalcCellSizes", "%s",
                  calcHeights ? "true" : "false");

   // Since Table::getAvailWidthOfChild does not calculate the column
   // widths, and so initially a random value (100) is returned, a
   // correction is necessary. The old values are temporary preserved
   // ...

   lout::misc::SimpleVector<int> oldColWidths (8);
   oldColWidths.setSize (colWidths->size ());
   colWidths->copyTo (&oldColWidths);
   
   actuallyCalcCellSizes (calcHeights);

   // ... and then compared to the new ones. In case of a difference,
   // the cell is told about this.

   for (int col = 0; col < colWidths->size (); col++) {
      if (oldColWidths.get (col) != colWidths->get (col)) {
         for (int row = 0; row < numRows; row++) {
            int n = row * numCols + col, col2;
            Child *child = children->get(n);
            if (child) {
               Widget *cell;
               switch (child->type) {
               case Child::CELL:
                  cell = child->cell.widget;
                  break;

               case Child::SPAN_SPACE:
                  // TODO Are Child::spanSpace::startRow and
                  // Child::spanSpace::startCol not defined?

                  // Search for actual cell. If not found, this means
                  // that a cell is spanning multiple columns *and*
                  // rows; in this case it has been processed before.

                  cell = NULL;
                  for (col2 = col - 1; col2 >= 0 && cell == NULL; col2--) {
                     int n2 = row * numCols + col2;
                     Child *child2 = children->get(n2);
                     if (child2 != NULL && child2->type == Child::CELL)
                        cell = child2->cell.widget;
                  }
                  break;

               default:
                  misc::assertNotReached ();
                  cell = NULL;
               }
                  
               if (cell)
                  cell->containerSizeChanged ();
            }
         }
      }
   }

   DBG_OBJ_LEAVE ();
}

void Table::actuallyCalcCellSizes (bool calcHeights)
{
   DBG_OBJ_ENTER ("resize", 0, "actuallyCalcCellSizes", "%s",
                  calcHeights ? "true" : "false");

   int childHeight;
   core::Extremes extremes;

   // Will also call forceCalcColumnExtremes(), when needed.
   getExtremes (&extremes);

   int availWidth = getAvailWidth (true);
   // When adjust_table_min_width is set, use perhaps the adjustment
   // width for correction. (TODO: Is this necessary?)
   int corrWidth =
      Table::getAdjustTableMinWidth () ? extremes.adjustmentWidth : 0;
   int totalWidth = misc::max (availWidth, corrWidth)
      - ((numCols + 1) * getStyle()->hBorderSpacing + boxDiffWidth ());
      
   DBG_OBJ_MSGF ("resize", 1,
                 "totalWidth = max (%d, %d) - ((%d - 1) * %d + %d) = <b>%d</b>",
                 availWidth, corrWidth, numCols, getStyle()->hBorderSpacing,
                 boxDiffWidth (), totalWidth);

   assert (colWidths->size () == numCols); // This is set in addCell.
   cumHeight->setSize (numRows + 1, 0);
   rowSpanCells->setSize (0);
   baseline->setSize (numRows);

   misc::SimpleVector<int> *oldColWidths = colWidths;
   colWidths = new misc::SimpleVector <int> (8);
   colWidths->setSize (numCols);

   int minWidth = 0, minWidthIntrinsic = 0, maxWidth = 0;
   for (int col = 0; col < colExtremes->size(); col++) {
      minWidth += colExtremes->getRef(col)->minWidth;
      minWidthIntrinsic += colExtremes->getRef(col)->minWidthIntrinsic;
      maxWidth += colExtremes->getRef(col)->maxWidth;
   }

   // CSS 'width' defined and effective?
   bool totalWidthSpecified = false;
   if (getStyle()->width != core::style::LENGTH_AUTO) {
      // Even if 'width' is defined, it may not have a defined value. We try
      // this trick (should perhaps be replaced by a cleaner solution):
      core::Requisition testReq = { -1, -1, -1 };
      correctRequisition (&testReq, core::splitHeightPreserveDescent, true,
                          false);
      if (testReq.width != -1)
         totalWidthSpecified = true;
   }

   DBG_OBJ_MSGF ("resize", 1,
                 "minWidth = %d, minWidthIntrinsic = %d, maxWidth %d, "
                 "totalWidth = %d, %s",
                 minWidth, minWidthIntrinsic, maxWidth, totalWidth,
                 totalWidthSpecified ? "specified" : "not specified");

   if (minWidth > totalWidth) {
      DBG_OBJ_MSG ("resize", 1, "case 1: minWidth > totalWidth");

      // The sum of all column minima is larger than the available
      // width, so we narrow the columns (see also CSS2 spec,
      // section 17.5, #6). We use a similar apportioning, but not
      // bases on minimal and maximal widths, but on intrinsic minimal
      // widths and corrected minimal widths. This way, intrinsic
      // extremes are preferred (so avoiding columns too narrow for
      // the actual contents), at the expenses of corrected ones
      // (which means that sometimes CSS values are handled
      // incorrectly).

      // A special case is a table with columns whose widths are
      // defined by percentage values. In this case, all other columns
      // are applied the intrinsic minimal width, while larger values
      // are applied to the columns with percentage width (but not
      // larger than the corrected width). The left columns are
      // preferred, but it is ensured that no column is narrower than
      // the intrinsic minimum.
      //
      // Example two columns with both "width: 70%" will be displayed like
      // this:
      //
      // --------------------------------------------------
      // |                                 |              |
      // --------------------------------------------------
      //
      // The first gets indeed 70% of the total width, the second only
      // the rest.
      //
      // This somewhat strange behaviour tries to mimic the somewhat
      // strange behaviour of Firefox and Chromium.

      if (numColWidthPercentage == 0 || minWidthIntrinsic >= totalWidth) {
         // Latter case (minWidthIntrinsic >= totalWidth): special treating
         // of percentage values would not make sense.

         DBG_OBJ_MSG ("resize", 1, "case 1a: simple apportioning");

         apportion2 (totalWidth, 0, colExtremes->size() - 1, MIN_MIN, MAX_MIN,
                     NULL, colWidths, 0);
      } else {
         DBG_OBJ_MSG ("resize", 1, "case 1b: treat percentages specially");

         // Keep track of the width which is apportioned to the rest
         // of the columns with percentage width (widthPartPer), and
         // the minimal width (intrinsic minimum) which is needed for
         // the rest of these columns (minWidthIntrinsicPer).

         int widthPartPer = totalWidth, minWidthIntrinsicPer = 0;
         for (int col = 0; col < colExtremes->size(); col++)
            if (colWidthPercentage->get (col))
               minWidthIntrinsicPer +=
                  colExtremes->getRef(col)->minWidthIntrinsic;
            else
               // Columns without percentage width get only the
               // intrinsic minimal, so subtract this from the width for the
               // columns *with* percentage
               widthPartPer -=
                  colExtremes->getRef(col)->minWidthIntrinsic;

         DBG_OBJ_MSGF ("resize", 1,
                       "widthPartPer = %d, minWidthIntrinsicPer = %d",
                       widthPartPer, minWidthIntrinsicPer);

         for (int col = 0; col < colExtremes->size(); col++)
            if (colWidthPercentage->get (col)) {
               int colWidth = colExtremes->getRef(col)->minWidth;
               int minIntr = colExtremes->getRef(col)->minWidthIntrinsic;

               minWidthIntrinsicPer -= minIntr;

               if (colWidth > widthPartPer - minWidthIntrinsicPer)
                  colWidth = widthPartPer - minWidthIntrinsicPer;

               colWidths->set (col, colWidth);
               widthPartPer -= colWidth;

               DBG_OBJ_MSGF ("resize", 1,
                             "#%d: colWidth = %d ... widthPartPer = %d, "
                             "minWidthIntrinsicPer = %d",
                             col, colWidth, widthPartPer, minWidthIntrinsicPer);

            } else
               colWidths->set (col,
                               colExtremes->getRef(col)->minWidthIntrinsic);

      }
   } else if (totalWidthSpecified && totalWidth > maxWidth) {
      DBG_OBJ_MSG ("resize", 1,
                   "case 2: totalWidthSpecified && totalWidth > maxWidth");

      // The width is specified (and so enforced), but all maxima sum
      // up to less than this specified width. The columns will have
      // there maximal width, and the extra space is apportioned
      // according to the column widths, and so to the column
      // maxima. This is done by simply passing MAX twice to the
      // apportioning function.

      // When column widths are specified (numColWidthSpecified > 0,
      // as calculated in forceCalcColumnExtremes()), they are treated
      // specially and excluded from the apportioning, so that the
      // specified column widths are enforced. An exception is when
      // all columns are specified: in this case they must be
      // enlargened to fill the whole table width.

      if (numColWidthSpecified == 0 ||
          numColWidthSpecified == colExtremes->size()) {
         DBG_OBJ_MSG ("resize", 1,
                      "subcase 2a: no or all columns with specified width");
         apportion2 (totalWidth, 0, colExtremes->size() - 1, MAX, MAX, NULL,
                     colWidths, 0);
      } else {
         DBG_OBJ_MSGF ("resize", 1,
                       "subcase 2b: %d column(s) with specified width",
                       numColWidthSpecified);

         // Separate columns with specified and unspecified width, and
         // apply apportion2() only to the latter.

         int numNotSpecified = colExtremes->size() - numColWidthSpecified;

         misc::SimpleVector<int> widthsNotSpecified (numNotSpecified);
         widthsNotSpecified.setSize (numNotSpecified);
         misc::SimpleVector<int> apportionDest (numNotSpecified);

         int totalWidthNotSpecified = totalWidth, indexNotSpecified = 0;
         for (int col = 0; col < colExtremes->size(); col++)
            if (colWidthSpecified->get (col))
               totalWidthNotSpecified -= colExtremes->getRef(col)->maxWidth;
            else {
               widthsNotSpecified.set (indexNotSpecified,
                                       colExtremes->getRef(col)->maxWidth);
               indexNotSpecified++;
            }

         DBG_IF_RTFL {
            DBG_OBJ_MSGF ("resize", 1, "totalWidthNotSpecified = %d",
                          totalWidthNotSpecified);

            DBG_OBJ_MSG ("resize", 1, "widthsNotSpecified:");
            DBG_OBJ_MSG_START ();

            for (int i = 0; i < widthsNotSpecified.size (); i++)
               DBG_OBJ_MSGF ("resize", 1, "#%d: %d",
                             i, widthsNotSpecified.get (i));

            DBG_OBJ_MSG_END ();
         }

         apportion2 (totalWidthNotSpecified, 0, numNotSpecified - 1, DATA, DATA,
                     (void*)&widthsNotSpecified, &apportionDest, 0);

         DBG_IF_RTFL {
            DBG_OBJ_MSG ("resize", 1, "apportionDest:");
            DBG_OBJ_MSG_START ();

            for (int i = 0; i < apportionDest.size (); i++)
               DBG_OBJ_MSGF ("resize", 1, "#%d: %d", i, apportionDest.get (i));

            DBG_OBJ_MSG_END ();
         }

         DBG_OBJ_MSG ("resize", 1, "finally setting column widths:");
         DBG_OBJ_MSG_START ();

         indexNotSpecified = 0;
         for (int col = 0; col < colExtremes->size(); col++)
            if (colWidthSpecified->get (col)) {
               DBG_OBJ_MSGF ("resize", 1, "#%d: specified, gets maximum %d",
                             col, colExtremes->getRef(col)->maxWidth);
               colWidths->set (col, colExtremes->getRef(col)->maxWidth);
            } else {
               DBG_OBJ_MSGF ("resize", 1, "#%d: not specified, gets value %d "
                             "at position %d from temporary list",
                             col, apportionDest.get (indexNotSpecified),
                             indexNotSpecified);
               colWidths->set (col, apportionDest.get (indexNotSpecified));
               indexNotSpecified++;
            }

         DBG_OBJ_MSG_END ();
      }
   } else {
      // Normal apportioning.
      int width =
         totalWidthSpecified ? totalWidth : misc::min (totalWidth, maxWidth);
      DBG_OBJ_MSGF ("resize", 1, "case 3: else; width = %d", width);
      apportion2 (width, 0, colExtremes->size() - 1, MIN, MAX, NULL, colWidths,
                  0);
   }

   // TODO: Adapted from old inline function "setColWidth". But (i) is
   // this anyway correct (col width is is not x)? And does the
   // performance gain actually play a role?
   for (int col = 0; col < colExtremes->size(); col++) {
      if (colWidths->get (col) != oldColWidths->get (col))
         redrawX = lout::misc::min (redrawX, colWidths->get (col));
   }

   DBG_IF_RTFL {
      DBG_OBJ_SET_NUM ("colWidths.size", colWidths->size ());
      for (int i = 0; i < colWidths->size (); i++)
         DBG_OBJ_ARRSET_NUM ("colWidths", i, colWidths->get (i));
   }

   colWidthsUpToDateWidthColExtremes = true;
   DBG_OBJ_SET_BOOL ("colWidthsUpToDateWidthColExtremes",
                     colWidthsUpToDateWidthColExtremes);

   for (int col = 0; col < numCols; col++) {
      if (col >= oldColWidths->size () || col >= colWidths->size () ||
          oldColWidths->get (col) != colWidths->get (col)) {
         // Column width has changed, tell children about this.
         for (int row = 0; row < numRows; row++) {
            int n = row * numCols + col;
            // TODO: Columns spanning several rows are only regarded
            // when the first column is affected.
            if (childDefined (n))
               children->get(n)->cell.widget->containerSizeChanged ();
         }
      }
   }

   delete oldColWidths;

   if (calcHeights) {
      setCumHeight (0, 0);
      for (int row = 0; row < numRows; row++) {
         /**
          * \bug dw::Table::baseline is not filled.
          */
         int rowHeight = 0;

         for (int col = 0; col < numCols; col++) {
            int n = row * numCols + col;
            if (childDefined (n)) {
               /* FIXME: Variable width is not used */
#if 0
               int width = (children->get(n)->cell.colspanEff - 1)
                  * getStyle()->hBorderSpacing;
               for (int i = 0; i < children->get(n)->cell.colspanEff; i++)
                  width += colWidths->get (col + i);
#endif

               core::Requisition childRequisition;
               //children->get(n)->cell.widget->setWidth (width);
               children->get(n)->cell.widget->sizeRequest (&childRequisition);
               childHeight = childRequisition.ascent + childRequisition.descent;
               if (children->get(n)->cell.rowspan == 1) {
                  rowHeight = misc::max (rowHeight, childHeight);
               } else {
                  rowSpanCells->increase();
                  rowSpanCells->set(rowSpanCells->size()-1, n);
               }
            }
         } // for col

         setCumHeight (row + 1,
            cumHeight->get (row) + rowHeight + getStyle()->vBorderSpacing);
      } // for row

      apportionRowSpan ();
   }

   DBG_OBJ_LEAVE ();
}

void Table::apportionRowSpan ()
{
   DBG_OBJ_ENTER0 ("resize", 0, "apportionRowSpan");

   int *rowHeight = NULL;

   for (int c = 0; c < rowSpanCells->size(); ++c) {
      int n = rowSpanCells->get(c);
      int row = n / numCols;
      int rs = children->get(n)->cell.rowspan;
      int sumRows = cumHeight->get(row+rs) - cumHeight->get(row);
      core::Requisition childRequisition;
      children->get(n)->cell.widget->sizeRequest (&childRequisition);
      int spanHeight = childRequisition.ascent + childRequisition.descent
                       + getStyle()->vBorderSpacing;
      if (sumRows >= spanHeight)
         continue;

      // Cell size is too small.
      _MSG("Short cell %d, sumRows=%d spanHeight=%d\n",
          n,sumRows,spanHeight);

      // Fill height array
      if (!rowHeight) {
         rowHeight = new int[numRows];
         for (int i = 0; i < numRows; i++)
            rowHeight[i] = cumHeight->get(i+1) - cumHeight->get(i);
      }
#ifdef DBG
      MSG(" rowHeight { ");
      for (int i = 0; i < numRows; i++)
         MSG("%d ", rowHeight[i]);
      MSG("}\n");
#endif

      // Calc new row sizes for this span.
      int cumHnew_i = 0, cumh_i = 0, hnew_i;
      for (int i = row; i < row + rs; ++i) {
         hnew_i =
            sumRows == 0 ? (int)((float)(spanHeight-cumHnew_i)/(row+rs-i)) :
            (sumRows-cumh_i) <= 0 ? 0 :
            (int)((float)(spanHeight-cumHnew_i)*rowHeight[i]/(sumRows-cumh_i));

         _MSG(" i=%-3d h=%d hnew_i=%d =%d*%d/%d   cumh_i=%d cumHnew_i=%d\n",
             i,rowHeight[i],hnew_i,
             spanHeight-cumHnew_i,rowHeight[i],sumRows-cumh_i,
             cumh_i, cumHnew_i);

         cumHnew_i += hnew_i;
         cumh_i += rowHeight[i];
         rowHeight[i] = hnew_i;
      }
      // Update cumHeight
      for (int i = 0; i < numRows; ++i)
         setCumHeight (i+1, cumHeight->get(i) + rowHeight[i]);
   }
   delete[] rowHeight;

   DBG_OBJ_LEAVE ();
}


/**
 * \brief Fills dw::Table::colExtremes in all cases.
 */
void Table::forceCalcColumnExtremes ()
{
   DBG_OBJ_ENTER0 ("resize", 0, "forceCalcColumnExtremes");

   if (numCols > 0) {
      lout::misc::SimpleVector<int> colSpanCells (8);
      colExtremes->setSize (numCols);
      colWidthSpecified->setSize (numCols);
      colWidthPercentage->setSize (numCols);

      // 1. cells with colspan = 1
      for (int col = 0; col < numCols; col++) {
         DBG_OBJ_MSGF ("resize", 1, "column %d", col);
         DBG_OBJ_MSG_START ();

         colWidthSpecified->set (col, false);
         colWidthPercentage->set (col, false);

         colExtremes->getRef(col)->minWidth = 0;
         colExtremes->getRef(col)->minWidthIntrinsic = 0;
         colExtremes->getRef(col)->maxWidth = 0;
         colExtremes->getRef(col)->maxWidthIntrinsic = 0;
         colExtremes->getRef(col)->adjustmentWidth = 0;

         for (int row = 0; row < numRows; row++) {
            DBG_OBJ_MSGF ("resize", 1, "row %d", row);
            DBG_OBJ_MSG_START ();

            int n = row * numCols + col;

            if (childDefined (n)) {
               if (children->get(n)->cell.colspanEff == 1) {
                  core::Extremes cellExtremes;
                  children->get(n)->cell.widget->getExtremes (&cellExtremes);

                  DBG_OBJ_MSGF ("resize", 1, "child: %d / %d",
                                cellExtremes.minWidth, cellExtremes.maxWidth);

                  colExtremes->getRef(col)->minWidthIntrinsic =
                     misc::max (colExtremes->getRef(col)->minWidthIntrinsic,
                                cellExtremes.minWidthIntrinsic);
                  colExtremes->getRef(col)->maxWidthIntrinsic =
                     misc::max (colExtremes->getRef(col)->minWidthIntrinsic,
                                colExtremes->getRef(col)->maxWidthIntrinsic,
                                cellExtremes.maxWidthIntrinsic);

                  colExtremes->getRef(col)->minWidth =
                     misc::max (colExtremes->getRef(col)->minWidth,
                                cellExtremes.minWidth);
                  colExtremes->getRef(col)->maxWidth =
                     misc::max (colExtremes->getRef(col)->minWidth,
                                colExtremes->getRef(col)->maxWidth,
                                cellExtremes.maxWidth);

                  colExtremes->getRef(col)->adjustmentWidth =
                     misc::max (colExtremes->getRef(col)->adjustmentWidth,
                                cellExtremes.adjustmentWidth);

                  core::style::Length childWidth =
                     children->get(n)->cell.widget->getStyle()->width;
                  if (childWidth != core::style::LENGTH_AUTO) {
                     colWidthSpecified->set (col, true);
                     if (core::style::isPerLength (childWidth))
                        colWidthPercentage->set (col, true);
                  }

                  DBG_OBJ_MSGF ("resize", 1, "column: %d / %d (%d / %d)",
                                colExtremes->getRef(col)->minWidth,
                                colExtremes->getRef(col)->maxWidth,
                                colExtremes->getRef(col)->minWidthIntrinsic,
                                colExtremes->getRef(col)->maxWidthIntrinsic);
               } else {
                  colSpanCells.increase ();
                  colSpanCells.setLast (n);
               }
            }

            DBG_OBJ_MSG_END ();
         }

         DBG_OBJ_MSG_END ();
      }

      // 2. cells with colspan > 1

      // TODO: Is this old comment still relevant? "If needed, here we
      // set proportionally apportioned col maximums."

      for (int i = 0; i < colSpanCells.size(); i++) {
         int n = colSpanCells.get (i);
         int col = n % numCols;
         int cs = children->get(n)->cell.colspanEff;

         core::Extremes cellExtremes;
         children->get(n)->cell.widget->getExtremes (&cellExtremes);

         calcExtremesSpanMultiCols (col, cs, &cellExtremes, MIN, MAX, NULL);
         calcExtremesSpanMultiCols (col, cs, &cellExtremes, MIN_INTR, MAX_INTR,
                                    NULL);
         calcAdjustmentWidthSpanMultiCols (col, cs, &cellExtremes);

         core::style::Length childWidth =
            children->get(n)->cell.widget->getStyle()->width;
         if (childWidth != core::style::LENGTH_AUTO) {
            for (int j = 0; j < cs; j++)
               colWidthSpecified->set (col + j, true);
            if (core::style::isPerLength (childWidth))
               for (int j = 0; j < cs; j++)
                  colWidthPercentage->set (col + j, true);
         }
      }
   }

   numColWidthSpecified = 0;
   numColWidthSpecified = 0;
   for (int i = 0; i < colExtremes->size (); i++) {
      if (colWidthSpecified->get (i))
         numColWidthSpecified++;
      if (colWidthPercentage->get (i))
         numColWidthPercentage++;
   }

   DBG_IF_RTFL {
      DBG_OBJ_SET_NUM ("colExtremes.size", colExtremes->size ());
      for (int i = 0; i < colExtremes->size (); i++) {
         DBG_OBJ_ARRATTRSET_NUM ("colExtremes", i, "minWidth",
                                 colExtremes->get(i).minWidth);
         DBG_OBJ_ARRATTRSET_NUM ("colExtremes", i, "minWidthIntrinsic",
                                 colExtremes->get(i).minWidthIntrinsic);
         DBG_OBJ_ARRATTRSET_NUM ("colExtremes", i, "maxWidth",
                                 colExtremes->get(i).maxWidth);
         DBG_OBJ_ARRATTRSET_NUM ("colExtremes", i, "maxWidthIntrinsic",
                                 colExtremes->get(i).maxWidthIntrinsic);
      }

      DBG_OBJ_SET_NUM ("colWidthSpecified.size", colWidthSpecified->size ());
      for (int i = 0; i < colWidthSpecified->size (); i++)
         DBG_OBJ_ARRSET_BOOL ("colWidthSpecified", i,
                              colWidthSpecified->get(i));
      DBG_OBJ_SET_NUM ("numColWidthSpecified", numColWidthSpecified);

      DBG_OBJ_SET_NUM ("colWidthPercentage.size", colWidthPercentage->size ());
      for (int i = 0; i < colWidthPercentage->size (); i++)
         DBG_OBJ_ARRSET_BOOL ("colWidthPercentage", i,
                              colWidthPercentage->get(i));
      DBG_OBJ_SET_NUM ("numColWidthPercentage", numColWidthPercentage);
   }

   colWidthsUpToDateWidthColExtremes = false;
   DBG_OBJ_SET_BOOL ("colWidthsUpToDateWidthColExtremes",
                     colWidthsUpToDateWidthColExtremes);

   DBG_OBJ_LEAVE ();
}

void Table::calcExtremesSpanMultiCols (int col, int cs,
                                       core::Extremes *cellExtremes,
                                       ExtrMod minExtrMod, ExtrMod maxExtrMod,
                                       void *extrData)
{
   DBG_OBJ_ENTER ("resize", 0, "calcExtremesSpanMulteCols",
                  "%d, %d, ..., %s, %s, ...",
                  col, cs, getExtrModName (minExtrMod),
                  getExtrModName (maxExtrMod));

   int cellMin = getExtreme (cellExtremes, minExtrMod);
   int cellMax = getExtreme (cellExtremes, maxExtrMod);

   int minSumCols = 0, maxSumCols = 0;

   for (int j = 0; j < cs; j++) {
      minSumCols += getColExtreme (col + j, minExtrMod, extrData);
      maxSumCols += getColExtreme (col + j, maxExtrMod, extrData);
   }

   DBG_OBJ_MSGF ("resize", 1, "cs = %d, cell: %d / %d, sum: %d / %d\n",
                 cs, cellMin, cellMax, minSumCols, maxSumCols);

   bool changeMin = cellMin > minSumCols;
   bool changeMax = cellMax > maxSumCols;
   if (changeMin || changeMax) {
      // TODO This differs from the documentation? Should work, anyway.
      misc::SimpleVector<int> newMin, newMax;
      if (changeMin)
         apportion2 (cellMin, col, col + cs - 1, MIN, MAX, NULL, &newMin, 0);
      if (changeMax)
         apportion2 (cellMax, col, col + cs - 1, MIN, MAX, NULL, &newMax, 0);

      for (int j = 0; j < cs; j++) {
         if (changeMin)
            setColExtreme (col + j, minExtrMod, extrData, newMin.get (j));
         if (changeMax)
            setColExtreme (col + j, maxExtrMod, extrData, newMax.get (j));

         // For cases where min and max are somewhat confused:
         setColExtreme (col + j, maxExtrMod, extrData,
                        misc::max (getColExtreme (col + j, minExtrMod,
                                                  extrData),
                                   getColExtreme (col + j, maxExtrMod,
                                                  extrData)));
      }
   }

   DBG_OBJ_LEAVE ();
}

void Table::calcAdjustmentWidthSpanMultiCols (int col, int cs,
                                              core::Extremes *cellExtremes)
{
   DBG_OBJ_ENTER ("resize", 0, "calcAdjustmentWidthSpanMultiCols",
                  "%d, %d, ...", col, cs);

   int sumAdjustmentWidth = 0;
   for (int j = 0; j < cs; j++)
      sumAdjustmentWidth +=  colExtremes->getRef(col + j)->adjustmentWidth;
   
   if (cellExtremes->adjustmentWidth > sumAdjustmentWidth) {
      misc::SimpleVector<int> newAdjustmentWidth;
      apportion2 (cellExtremes->adjustmentWidth, col, col + cs - 1, MIN, MAX,
                  NULL, &newAdjustmentWidth, 0);
      for (int j = 0; j < cs; j++)
         colExtremes->getRef(col + j)->adjustmentWidth =
            newAdjustmentWidth.get (j);
   }

   DBG_OBJ_LEAVE ();
}

/**
 * \brief Actual apportionment function.
 */
void Table::apportion2 (int totalWidth, int firstCol, int lastCol,
                        ExtrMod minExtrMod, ExtrMod maxExtrMod, void *extrData,
                        misc::SimpleVector<int> *dest, int destOffset)
{
   DBG_OBJ_ENTER ("resize", 0, "apportion2", "%d, %d, %d, %s, %s, ..., %d",
                  totalWidth, firstCol, lastCol, getExtrModName (minExtrMod),
                  getExtrModName (maxExtrMod), destOffset);

   if (lastCol >= firstCol) {
      dest->setSize (destOffset + lastCol - firstCol + 1, 0);

      int totalMin = 0, totalMax = 0;
      for (int col = firstCol; col <= lastCol; col++) {
         totalMin += getColExtreme (col, minExtrMod, extrData);
         totalMax += getColExtreme (col, maxExtrMod, extrData);
      }

      DBG_OBJ_MSGF ("resize", 1,
                    "totalWidth = %d, totalMin = %d, totalMax = %d",
                    totalWidth, totalMin, totalMax);

      // The actual calculation is rather simple, the ith value is:
      //
      //
      //                     (max[i] - min[i]) * (totalMax - totalMin)
      // width[i] = min[i] + -----------------------------------------
      //                              (totalWidth - totalMin)
      //
      // (Regard "total" as "sum".) With the following general
      // definitions (for both the list and sums):
      //
      //    diffExtr = max - min
      //    diffWidth = width - min
      //
      // it is simplified to:
      //
      //                   diffExtr[i] * totalDiffWidth
      //    diffWidth[i] = ----------------------------
      //                           totalDiffExtr
      //
      // Of course, if totalDiffExtr is 0, this is not defined;
      // instead, we apportion according to the minima:
      //
      //               min[i] * totalWidth
      //    width[i] = -------------------
      //                    totalMin
      //
      // Since min[i] <= max[i] for all i, totalMin == totalMax
      // implies that min[i] == max[i] for all i.
      //
      // Third, it totalMin == 0 (which also implies min[i] = max[i] = 0),
      // the result is
      //
      //    width[i] = totalWidth / n

      int totalDiffExtr = totalMax - totalMin;
      if (totalDiffExtr != 0) {
         // Normal case. The algorithm described in
         // "rounding-errors.doc" is used, with:
         //
         //    x[i] = diffExtr[i]
         //    y[i] = diffWidth[i]
         //    a = totalDiffWidth
         //    b = totalDiffExtr

         DBG_OBJ_MSG ("resize", 1, "normal case");

         int totalDiffWidth = totalWidth - totalMin;
         int cumDiffExtr = 0, cumDiffWidth = 0;

         for (int col = firstCol; col <= lastCol; col++) {
            int min = getColExtreme (col, minExtrMod, extrData);
            int max = getColExtreme (col, maxExtrMod, extrData);
            int diffExtr = max - min;

            cumDiffExtr += diffExtr;
            int diffWidth =
               (cumDiffExtr * totalDiffWidth) / totalDiffExtr - cumDiffWidth;
            cumDiffWidth += diffWidth;

            dest->set (destOffset - firstCol + col, diffWidth + min);
         }
      } else if (totalMin != 0) {
         // Special case. Again, same algorithm, with
         //
         //    x[i] = min[i]
         //    y[i] = width[i]
         //    a = totalWidth
         //    b = totalMin

         DBG_OBJ_MSG ("resize", 1, "special case 1");

         int cumMin = 0, cumWidth = 0;
         for (int col = firstCol; col <= lastCol; col++) {
            int min = getColExtreme (col, minExtrMod, extrData);
            cumMin += min;
            int width = (cumMin * totalWidth) / totalMin - cumWidth;
            cumWidth += width;

            dest->set (destOffset - firstCol + col, width);
         }
      } else { // if (totalMin == 0)
         // Last special case. Same algorithm, with
         //
         //    x[i] = 1 (so cumX = i = col - firstCol + 1)
         //    y[i] = width[i]
         //    a = totalWidth
         //    b = n = lastCol - firstCol + 1

         DBG_OBJ_MSG ("resize", 1, "special case 2");

         int cumWidth = 0, n = (lastCol - firstCol + 1);
         for (int col = firstCol; col <= lastCol; col++) {
            int i = (col - firstCol + 1);
            int width = (i * totalWidth) / n - cumWidth;
            cumWidth += width;

            dest->set (destOffset - firstCol + col, width);
         }
      }
   }

   DBG_OBJ_LEAVE ();
}

} // namespace dw
