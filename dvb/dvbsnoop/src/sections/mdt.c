/*
$Id: mdt.c,v 1.3 2004/09/01 20:20:34 rasc Exp $


 DVBSNOOP

 a dvb sniffer  and mpeg2 stream analyzer tool
 http://dvbsnoop.sourceforge.net/

 (c) 2001-2004   Rainer.Scherg@gmx.de  (rasc)


 -- MDT section  (MetaData Section)



$Log: mdt.c,v $
Revision 1.3  2004/09/01 20:20:34  rasc
new cmdline option: -buffersize KB  (set demux buffersize in KBytes)

Revision 1.2  2004/08/24 21:30:23  rasc
more Metadata

Revision 1.1  2004/08/22 18:36:45  rasc
 - Bugfix: multilang service descriptor fix  (tnx to Karsten Siebert)
 - New: MetaData Section  (Basic) (H.222.0 AMD1)



*/




#include "dvbsnoop.h"
#include "mdt.h"
#include "descriptors/descriptor.h"
#include "strings/dvb_str.h"
#include "misc/output.h"




void decode_MDT (u_char *b, int len)
{
  /* IS13818-1  AMD1 */

  int   table_id;
  int   m_len;
  int   sfi;	
  int   x;


  out_nl (3,"MDT-decoding....");
  table_id = outBit_S2x_NL (3,"Table_ID: ",  b,  0, 8,
                 (char *(*)(u_long)) dvbstrTableID );     
  if (table_id != 0x06) {
    out_nl (3,"wrong Table ID");
    return;
  }



  outBit_Sx_NL (3,"section_syntax_indicator: ",		b,  8, 1);
  outBit_Sx_NL (3,"private_indicator: ",		b,  9, 1);
  x = outBit_Sx(3,"random_access_indicator: ",		b, 10, 1);
	if (x == 1) {
		out (3," [= access point to the metadata]");
	}
	out_NL(3);


  x = outBit_Sx (3,"decoder_config_flag: ",		b, 11, 1);
	if (x == 1) {
		out (3," [= decoder configuration information is present in the metadata Access Unit]");
	}
	out_NL(3);



  m_len = outBit_Sx_NL (5,"metadata_section_length: ",	b, 12, 12);

  outBit_Sx_NL (3,"metadata_service_id: ",		b, 24,  8);
  outBit_Sx_NL (6,"reserved: ",				b, 32,  8);

  sfi = outBit_S2x_NL (3,"section_fragment_indication: ",b, 40,  2,
                 (char *(*)(u_long)) dvbstrMPEG_metadata_section_frag_indication );

  outBit_Sx_NL (3,"version_number: ",			b, 42,  5);
  outBit_S2x_NL(3,"current_next_indicator: ",		b, 47,  1,
                 (char *(*)(u_long)) dvbstrCurrentNextIndicator );     
  

  outBit_Sx_NL (3,"Section_number: ",			b, 48,  8);
  outBit_Sx_NL (3,"Last_section_number: ",		b, 54,  8);

  b     += 8;
  m_len -= 5;


  print_databytes (4,"metadata_byte:", b,m_len-4);   // $$$ TODO  MetaData AU_CELL etc
  b     += m_len-4;


  outBit_Sx_NL (5,"CRC: ",				b,  0, 32);
}





/*
 *  $$$ TODO

metadata_byte: This 8-bit contains contiguous bytes from a metadata Access Unit.
  Q: so is it a contigous stream splitted over serveral sections?
  A: guess so...




Table Amd.1-10 � Metadata Access Unit Wrapper

Metadata_AU_wrapper () {
	for (i=0; i<N;i++){
		Metadata_AU_cell ()
	}
}


Table Amd.1-11 � Metadata AU cell

Metadata_AU_cell () {
	metadata_service_id
	sequence_number
	cell_fragment_indication
	decoder_config_flag
	random_access_indicator
	reserved
	AU_cell_data_length
	for (i=0; I<AU_cell_data_length;i++){
		AU_cell_data_byte
	}
}


*/

