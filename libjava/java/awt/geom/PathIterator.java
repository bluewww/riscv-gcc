/* Copyright (C) 2000, 2002  Free Software Foundation

This file is part of GNU Classpath.

GNU Classpath is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Classpath is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Classpath; see the file COPYING.  If not, write to the
Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
02111-1307 USA.

As a special exception, if you link this library with other files to
produce an executable, this library does not by itself cause the
resulting executable to be covered by the GNU General Public License.
This exception does not however invalidate any other reasons why the
executable file might be covered by the GNU General Public License. */

package java.awt.geom;

/**
 * @author Tom Tromey <tromey@cygnus.com>
 * @date April 16, 2000
 */

public interface PathIterator
{
  public static final int SEG_CLOSE = 4;
  public static final int SEG_CUBICTO = 3;
  public static final int SEG_LINETO = 1;
  public static final int SEG_MOVETO = 0;
  public static final int SEG_QUADTO = 2;
  public static final int WIND_EVEN_ODD = 0;
  public static final int WIND_NON_ZERO = 1;

  public int currentSegment (double[] coords);
  public int currentSegment (float[] coords);
  public int getWindingRule ();
  public boolean isDone ();
  public void next ();
}
