/*****************************************************************************
*   Gnome Wave Cleaner Version 0.19
*   Copyright (C) 2003 Jeffrey J. Welty
*   
*   This program is free software; you can redistribute it and/or
*   modify it under the terms of the GNU General Public License
*   as published by the Free Software Foundation; either version 2
*   of the License, or (at your option) any later version.
*   
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*   
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*******************************************************************************/

/* gwc audio device interface impl.  ...frank 12.09.03 */

#ifdef HAVE_ALSA
# include "audio_alsa.c"
#else
# ifdef HAVE_PULSE_AUDIO
#  include "audio_pa.c"
# else
#  ifdef MAC_OS_X /* MacOSX */
#   include "audio_osx.c"
#  else
#   include "audio_oss.c"
#  endif 
# endif 
#endif
