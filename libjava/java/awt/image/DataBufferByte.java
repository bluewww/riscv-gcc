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

package java.awt.image;

/* This is one of several classes that are nearly identical. Maybe we
   should have a central template and generate all these files. This
   is one of the cases where templates or macros would have been
   useful to have in Java.

   This file has been created using search-replace. My only fear is
   that these classes will grow out-of-sync as of a result of changes
   that are not propagated to the other files. As always, mirroring
   code is a maintenance nightmare.  */

/**
 * @author Rolf W. Rasmussen <rolfwr@ii.uib.no>
 */
public class DataBufferByte extends DataBuffer
{
  private byte[] data;
  private byte[][] bankData;
  
  public DataBufferByte(int size)
  {
    super(TYPE_BYTE, size);
    data = new byte[size];
  }

  public DataBufferByte(int size, int numBanks)
  {
    super(TYPE_BYTE, size, numBanks);
    bankData = new byte[numBanks][size];
    data = bankData[0];
  }

  public DataBufferByte(byte[] dataArray, int size)
  {
    super(TYPE_BYTE, size);
    data = dataArray;
  }
    
  public DataBufferByte(byte[] dataArray, int size, int offset)
  {
    super(TYPE_BYTE, size, 1, offset);
    data = dataArray;
  }

  public DataBufferByte(byte[][] dataArray, int size)
  {
    super(TYPE_BYTE, size, dataArray.length);
    bankData = dataArray;
    data = bankData[0];
  }

  public DataBufferByte(byte[][] dataArray, int size, int[] offsets)
  {
    super(TYPE_BYTE, size, dataArray.length, offsets);
    bankData = dataArray;
    data = bankData[0];
  }

  public byte[] getData()
  {
    return data;
  }
    
  public byte[] getData(int bank) 
  {
    return bankData[bank];
  }
    
  public byte[][] getBankData()
  {
    return bankData;
  }
  
  public int getElem(int i)
  {
    return data[i+offset] & 0xff; // get unsigned byte as int
  }
  
  public int getElem(int bank, int i)
  {
    // get unsigned byte as int
    return bankData[bank][i+offsets[bank]] & 0xff;
  }

  public void setElem(int i, int val)
  {
    data[i+offset] = (byte) val;
  }

  public void setElem(int bank, int i, int val)
  {
    bankData[bank][i+offsets[bank]] = (byte) val;
  }
}
