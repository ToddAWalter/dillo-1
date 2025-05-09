#ifndef __DW_TYPES_HH__
#define __DW_TYPES_HH__

#ifndef __INCLUDED_FROM_DW_CORE_HH__
#   error Do not include this file directly, use "core.hh" instead.
#endif

namespace dw {
namespace core {

namespace style {
   class Style;
}

enum HPosition
{
   HPOS_LEFT,
   HPOS_CENTER,
   HPOS_RIGHT,
   HPOS_INTO_VIEW, /* scroll only, until the content in question comes
                    * into view */
   HPOS_NO_CHANGE
};

enum VPosition
{
   VPOS_TOP,
   VPOS_CENTER,
   VPOS_BOTTOM,
   VPOS_INTO_VIEW, /* scroll only, until the content in question comes
                    * into view */
   VPOS_NO_CHANGE
};

enum ScrollCommand {SCREEN_UP_CMD, SCREEN_DOWN_CMD, SCREEN_LEFT_CMD,
                    SCREEN_RIGHT_CMD, LINE_UP_CMD, LINE_DOWN_CMD,
                    LEFT_CMD, RIGHT_CMD, TOP_CMD, BOTTOM_CMD, NONE_CMD};

/*
 * Different "layers" may be highlighted in a widget.
 */
enum HighlightLayer
{
   HIGHLIGHT_SELECTION,
   HIGHLIGHT_FINDTEXT,
   HIGHLIGHT_NUM_LAYERS
};

struct Point
{
  int x;
  int y;
};

/**
 * \brief Abstract interface for different shapes.
 */
class Shape: public lout::object::Object
{
public:
   virtual bool isPointWithin (int x, int y) = 0;
   virtual void draw (core::View *view, core::style::Style *style, int x,
                      int y) = 0;
};

/**
 * \brief dw::core::Shape implementation for simple rectangles.
 */
class Rectangle: public Shape
{
public:
   int x;
   int y;
   int width;
   int height;

   inline Rectangle () { }
   Rectangle (int x, int y, int width, int height);

   void draw (core::View *view, core::style::Style *style, int x, int y);
   bool intersectsWith (Rectangle *otherRect, Rectangle *dest);
   bool isSubsetOf (Rectangle *otherRect);
   bool isPointWithin (int x, int y);
   bool isEmpty () { return width <= 0 || height <= 0; };
};

/**
 * \brief dw::core::Shape implementation for simple circles.
 */
class Circle: public Shape
{
public:
   int x, y, radius;

   Circle (int x, int y, int radius);

   void draw (core::View *view, core::style::Style *style, int x, int y);
   bool isPointWithin (int x, int y);
};

/**
 * \brief dw::core::Shape implementation for polygons.
 */
class Polygon: public Shape
{
private:
   lout::misc::SimpleVector<Point> *points;
   int minx, miny, maxx, maxy;

   /**
    * \brief Return the z-coordinate of the vector product of two
    *    vectors, whose z-coordinate is 0 (so that x and y of
    *    the vector product is 0, too).
    */
   inline int zOfVectorProduct(int x1, int y1, int x2, int y2) {
      return x1 * y2 - x2 * y1;
   }

   bool linesCross0(int ax1, int ay1, int ax2, int ay2,
                    int bx1, int by1, int bx2, int by2);
   bool linesCross(int ax1, int ay1, int ax2, int ay2,
                   int bx1, int by1, int bx2, int by2);

public:
   Polygon ();
   ~Polygon ();

   void draw (core::View *view, core::style::Style *style, int x, int y);
   void addPoint (int x, int y);
   bool isPointWithin (int x, int y);
};

/**
 * Implementation for a point set.
 * Currently represented as a set of rectangles not containing
 * each other.
 * It is guaranteed that the rectangles returned by rectangles ()
 * cover all rectangles that were added with addRectangle ().
 */
class Region
{
private:
   lout::container::typed::List <Rectangle> *rectangleList;

public:
   Region ();
   ~Region ();

   void clear () { rectangleList->clear (); };

   void addRectangle (Rectangle *r);

   lout::container::typed::Iterator <Rectangle> rectangles ()
   {
      return rectangleList->iterator ();
   };
};

/**
 * \brief Represents the allocation, i.e. actual position and size of a
 *    dw::core::Widget.
 */
struct Allocation
{
   int x;
   int y;
   int width;
   int ascent;
   int descent;
};

struct Requisition
{
   int width;
   int ascent;
   int descent;
};

struct Extremes
{
   int minWidth;
   int maxWidth;
   int minWidthIntrinsic;
   int maxWidthIntrinsic;
   int adjustmentWidth;
};

class WidgetReference: public lout::object::Object
{
public:
   Widget *widget;
   int parentRef;

   WidgetReference (Widget *widget) { parentRef = -1; this->widget = widget; }
};

struct Content
{
   enum Type {
      START             = 1 << 0,
      END               = 1 << 1,
      TEXT              = 1 << 2,

      /** \brief widget in normal flow, so that _this_ widget
          (containing this content) is both container (parent) and
          generator */
      WIDGET_IN_FLOW    = 1 << 3,

      /** \brief widget out of flow (OOF); _this_ widget (containing
          this content) is only the container (parent), but _not_
          generator */
      WIDGET_OOF_CONT    = 1 << 4,

      /** \brief reference to a widget out of flow (OOF); _this_
          widget (containing this content) is only the generator
          (parent), but _not_ container */
      WIDGET_OOF_REF    = 1 << 5,
      BREAK             = 1 << 6,

      /** \brief can be used internally, but should never be exposed,
          e. g. by iterators */
      INVALID            = 1 << 7,

      ALL               = 0xff,
      REAL_CONTENT      = 0xff ^ (START | END),
      SELECTION_CONTENT = TEXT | BREAK, // WIDGET_* must be set additionally
      ANY_WIDGET        = WIDGET_IN_FLOW | WIDGET_OOF_CONT | WIDGET_OOF_REF
   };

   /* Content is embedded in struct Word therefore we
    * try to be space efficient.
    */
   short type;
   bool space;
   union {
      const char *text;
      Widget *widget;
      WidgetReference *widgetReference;
      int breakSpace;
   };

   static Content::Type maskForSelection (bool followReferences);

   static void intoStringBuffer(Content *content, lout::misc::StringBuffer *sb);
   static void maskIntoStringBuffer(Type mask, lout::misc::StringBuffer *sb);
   static void print (Content *content);
   static void printMask (Type mask);

   inline Widget *getWidget () {
      assert (type & ANY_WIDGET);
      return type == WIDGET_OOF_REF ? widgetReference->widget : widget;
   }
};

/**
 * \brief Base class for dw::core::DrawingContext and
 *    dw::core::GettingWidgetAtPointContext.
 *
 * Not to be confused with the *stacking context* as defined by CSS.
 */
class StackingProcessingContext
{
private:
   lout::container::typed::HashSet<lout::object::TypedPointer<Widget> >
      *widgetsProcessedAsInterruption;

public:
   inline StackingProcessingContext () {
      widgetsProcessedAsInterruption =
         new lout::container::typed::HashSet<lout::object::
                                             TypedPointer<Widget> > (true);
   }
   
   inline ~StackingProcessingContext ()
   { delete widgetsProcessedAsInterruption; }

   inline bool hasWidgetBeenProcessedAsInterruption (Widget *widget) {
      lout::object::TypedPointer<Widget> key (widget);
      return widgetsProcessedAsInterruption->contains (&key);
   }

   inline void addWidgetProcessedAsInterruption (Widget *widget) {
      lout::object::TypedPointer<Widget> *key =
         new lout::object::TypedPointer<Widget> (widget);
      return widgetsProcessedAsInterruption->put (key);
   }
};

/**
 * \brief Set at the top when drawing.
 *
 * See \ref dw-interrupted-drawing for details.
 */
class DrawingContext: public StackingProcessingContext
{
private:
   Rectangle toplevelArea;

public:
   inline DrawingContext (Rectangle *toplevelArea) {
      this->toplevelArea = *toplevelArea;
   }
   
   inline Rectangle *getToplevelArea () { return &toplevelArea; }
};

/**
 * \brief Set at the top when getting the widget at the point.
 *
 * Similar to dw::core::DrawingContext.
 */
class GettingWidgetAtPointContext: public StackingProcessingContext
{
};

} // namespace core
} // namespace dw

#endif // __DW_TYPES_HH__
