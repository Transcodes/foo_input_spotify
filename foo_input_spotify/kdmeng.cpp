#include "kdmeng.h"

#define _USE_MATH_DEFINES
#include <math.h>

#define scale(a, b, c) MulDiv( a, b, c )

static _inline void fsin ( long * a )
{
	const float oneshr10 = 1.f / ( float ) ( 1 << 10 );
	const float oneshl14 = 1.f * ( float ) ( 1 << 14 );

	*a = ( long ) ( sin( ( float ) *a * M_PI * oneshr10 ) * oneshl14 );
}

static _inline void calcvolookupmono ( long * t, long a, long b )
{
	for ( unsigned i = 0; i < 256; i++ )
	{
		*t++ = a;
		a += b;
	}
}

static _inline void calcvolookupstereo ( long * t, long a, long b, long c, long d )
{
	for ( unsigned i = 0; i < 256; i++ )
	{
		*t++ = a;
		*t++ = c;
		a += b;
		c += d;
	}
}

static _inline long mulscale16 ( long a, long d )
{
	return ( long ) ( ( ( __int64 ) a * ( __int64 ) d ) >> 16 );
}

static _inline long mulscale24 ( long a, long d )
{
	return ( long ) ( ( ( __int64 ) a * ( __int64 ) d ) >> 24 );
}

static _inline long mulscale30 ( long a, long d )
{
	return ( long ) ( ( ( __int64 ) a * ( __int64 ) d ) >> 30 );
}

static _inline void clearbuf (void *d, long c, long a)
{
	unsigned long * dl = ( unsigned long * ) d;
	for ( unsigned i = 0; i < c; i++ ) *dl++ = a;
}

static _inline void copybuf (void *s, void *d, long c)
{
	unsigned long * sl = ( unsigned long * ) s;
	unsigned long * dl = ( unsigned long * ) d;
	for ( unsigned i = 0; i < c; i++ ) *dl++ = *sl++;
}

static void bound2char( unsigned count, long * in, unsigned char * out )
{
	for ( unsigned i = 0, j = count * 2; i < j; i++ )
	{
		long sample = *in >> 8;
		*in++ = 32768;
		if ( sample < 0 ) sample = 0;
		else if ( sample > 255 ) sample = 255;
		*out++ = sample;
	}
}

static void bound2short( unsigned count, long * in, unsigned char * out )
{
	signed short * outs = ( signed short * ) out;
	for ( unsigned i = 0, j = count * 2; i < j; i++ )
	{
		long sample = *in;
		*in++ = 32768;
		if ( sample < 0 ) sample = 0;
		else if ( sample > 65535 ) sample = 65535;
		*outs++ = sample ^ 0x8000;
	}
}

long kdmeng::monohicomb( long * b, long c, long d, long si, long * di )
{
	LARGE_INTEGER dasinc, sample_offset, interp;

	if ( si >= 0 ) return 0;

	dasinc.QuadPart = ( ( __int64 ) d ) << ( 32 - 12 );

	sample_offset.QuadPart = ( ( __int64 ) si ) << ( 32 - 12 );

	while ( c )
	{
		unsigned short sample = * ( unsigned short * ) ( kdmasm4 + sample_offset.HighPart );
		interp.QuadPart = sample_offset.LowPart;
		interp.QuadPart *= ( sample >> 8 ) - ( sample & 0xFF );
		d = interp.HighPart + sample & 0xFF;
		*di++ += b[ d ];
		sample_offset.QuadPart += dasinc.QuadPart;
		if ( sample_offset.QuadPart >= 0 )
		{
			if ( !kdmasm1 ) break;
			kdmasm4 = kdmasm2;
			sample_offset.QuadPart -= ( ( __int64 ) kdmasm3 ) << ( 32 - 12 );
		}
		c--;
	}

	return sample_offset.QuadPart >> ( 32 - 12 );
}

long kdmeng::stereohicomb( long * b, long c, long d, long si, long * di )
{
	LARGE_INTEGER dasinc, sample_offset, interp;

	if ( si >= 0 ) return 0;

	dasinc.QuadPart = ( ( __int64 ) d ) << ( 32 - 12 );

	sample_offset.QuadPart = ( ( __int64 ) si ) << ( 32 - 12 );

	while ( c )
	{
		unsigned short sample = * ( unsigned short * ) ( kdmasm4 + sample_offset.HighPart );
		interp.QuadPart = sample_offset.LowPart;
		interp.QuadPart *= ( sample >> 8 ) - ( sample & 0xFF );
		d = interp.HighPart + ( sample & 0xFF );
		*di++ += b[ d * 2 ];
		*di++ += b[ d * 2 + 1 ];
		sample_offset.QuadPart += dasinc.QuadPart;
		if ( sample_offset.QuadPart >= 0 )
		{
			if ( !kdmasm1 ) break;
			kdmasm4 = kdmasm2;
			sample_offset.QuadPart -= ( ( __int64 ) kdmasm3 ) << ( 32 - 12 );
		}
		c--;
	}

	return sample_offset.QuadPart >> ( 32 - 12 );
}

void kdmeng::musicon()
{
	notecnt = 0;
	timecount = nttime[notecnt];
	musicrepeat = 1;
	musicstatus = 1;
}

void kdmeng::musicoff()
{
	musicstatus = 0;
	for(unsigned i = 0; i < NUMCHANNELS; i++ ) splc [i] = 0;
	musicrepeat = 0;
	timecount = 0;
	notecnt = 0;
}

long kdmeng::loadwaves ( const char * refdir, abort_callback & p_abort )
{
	unsigned i, j, dawaversionum;
	pfc::string8 kwvnam;
	service_ptr_t<file> fil;

	if (snd.get_count()) return(1);

		//Look for WAVES.KWV in same directory as KDM file
	kwvnam = refdir;
	kwvnam.truncate( kwvnam.scan_filename() );
	kwvnam += "waves.kwv";

	filesystem::g_open( fil, kwvnam, filesystem::open_mode_read, p_abort );
	fil->read_lendian_t( dawaversionum, p_abort );
	if ( dawaversionum != 0 ) return 0;

	totsndbytes = 0;
	fil->read_lendian_t( numwaves, p_abort );
	for ( i = 0; i < numwaves; i++ )
	{
		fil->read_object_t( instname[i], p_abort );
		fil->read_lendian_t( wavleng[i], p_abort );
		fil->read_lendian_t( repstart[i], p_abort );
		fil->read_lendian_t( repleng[i], p_abort );
		fil->read_lendian_t( finetune[i], p_abort );
		wavoffs[i] = ( unsigned char * ) totsndbytes;
		totsndbytes += wavleng[i];
	}
	for( i = numwaves; i < MAXWAVES; i++ )
	{
		memset( instname[i], 0, sizeof( instname[i] ) );
		wavoffs[i] = ( unsigned char * ) totsndbytes;
		wavleng[i] = 0L;
		repstart[i] = 0L;
		repleng[i] = 0L;
		finetune[i] = 0L;
	}

	snd.set_count( totsndbytes + 2 );

	for ( i = 0; i < MAXWAVES; i++ ) wavoffs[ i ] = snd.get_ptr() + ( size_t ) wavoffs[ i ];
	fil->read_object( snd.get_ptr(), totsndbytes, p_abort );
	snd[ totsndbytes ] = snd[ totsndbytes + 1 ] = 128;
	return 1;
}

kdmeng::kdmeng( unsigned samplerate, unsigned numspeakers, unsigned bytespersample )
	: kdmsamplerate( samplerate ), kdmnumspeakers( numspeakers ), kdmbytespersample( bytespersample )
{
	long i, j, k;

	j = ( ( ( 11025L * 2093 ) / kdmsamplerate ) << 13 );
	for( i = 1; i < 76; i++ )
	{
		frqtable[i] = j;
		j = mulscale30( j, 1137589835 );  //(pow(2,1/12)<<30) = 1137589835
	}
	for( i = 0; i >= -14; i-- ) frqtable[ i & 255 ] = ( frqtable[ ( i + 12 ) & 255 ] >> 1 );

	timecount = notecnt = musicstatus = musicrepeat = 0;

	clearbuf( (void *)stemp, sizeof( stemp ) >> 2, 32768L );
	for( i = 0; i < ( kdmsamplerate >> 11 ); i++ )
	{
		j = 1536 - ( i << 10 ) / ( kdmsamplerate >> 11 );
		fsin( &j );
		ramplookup[ i ] = ( ( 16384 - j ) << 1 );
	}

	for( i = 0; i < 256; i++ )
	{
		j = i * 90; fsin( &j );
		eff[ 0 ][ i ] = 65536 + j / 9;
		eff[ 1 ][ i ] = min( 58386 + ( ( i * ( 65536 - 58386 ) ) / 30 ), 65536 );
		eff[ 2 ][ i ] = max( 69433 + ( ( i * ( 65536 - 69433 ) ) / 30 ), 65536 );
		j = ( i * 2048 ) / 120; fsin( &j );
		eff[ 3 ][ i ] = 65536 + ( j << 2 );
		j = ( i * 2048 ) / 30; fsin( &j );
		eff[ 4 ][ i ] = 65536 + j;
		switch( ( i >> 1 ) % 3 )
		{
			case 0: eff[ 5 ][ i ] = 65536; break;
			case 1: eff[ 5 ][ i ] = 65536 * 330 / 262; break;
			case 2: eff[ 5 ][ i ] = 65536 * 392 / 262; break;
		}
		eff[ 6 ][ i ] = min( ( i << 16 ) / 120, 65536 );
		eff[ 7 ][ i ] = max( 65536 - ( i << 16 ) / 120, 0 );
	}

	musicoff();
}

void kdmeng::startwave( long wavnum, long dafreq, long davolume1, long davolume2, long dafrqeff, long davoleff, long dapaneff )
{
	long i, j, chanum;

	if ( ( davolume1 | davolume2 ) == 0 ) return;

	chanum = 0;
	for( i = NUMCHANNELS - 1; i > 0; i-- )
		if ( splc[ i ] > splc[ chanum ] )
			chanum = i;

	splc[ chanum ] = 0;     //Disable channel temporarily for clean switch

	if ( kdmnumspeakers == 1 )
		calcvolookupmono( &volookup[ chanum << 9 ], -( davolume1 + davolume2 ) << 6, ( davolume1 + davolume2 ) >> 1 );
	else
		calcvolookupstereo( &volookup[ chanum << 9], -( davolume1 << 7 ), davolume1, -( davolume2 << 7 ), davolume2 );

	sinc[ chanum ] = dafreq;
	svol1[ chanum ] = davolume1;
	svol2[ chanum ] = davolume2;
	soff[ chanum ] = wavoffs[ wavnum ] + wavleng[ wavnum ];
	splc[ chanum ] = -( wavleng[ wavnum ] << 12 );              //splc's modified last
	swavenum[ chanum ] = wavnum;
	frqeff[ chanum ] = dafrqeff; frqoff[ chanum ] = 0;
	voleff[ chanum ] = davoleff; voloff[ chanum ] = 0;
	paneff[ chanum ] = dapaneff; panoff[ chanum ] = 0;
}

long kdmeng::load( service_ptr_t<file> & fil, const char * filename, abort_callback & p_abort )
{
	if ( !loadwaves( filename, p_abort ) ) return 0;
	if ( !snd.get_count() ) return 0;

	musicoff();

	kdmversionum = 0;
	numnotes = 0;
	numtracks = 0;
	memset( trinst, 0, sizeof( trinst ) );
	memset( trquant, 0, sizeof( trquant ) );
	memset( trvol1, 0, sizeof( trvol1 ) );
	memset( trvol2, 0, sizeof( trvol2 ) );
	memset( nttime, 0, sizeof( nttime ) );
	memset( nttrack, 0, sizeof( nttrack ) );
	memset( ntfreq, 0, sizeof( ntfreq ) );
	memset( ntvol1, 0, sizeof( ntvol1 ) );
	memset( ntvol2, 0, sizeof( ntvol2 ) );
	memset( ntfrqeff, 0, sizeof( ntfrqeff ) );
	memset( ntvoleff, 0, sizeof( ntvoleff ) );
	memset( ntpaneff, 0, sizeof( ntpaneff ) );

	fil->read_lendian_t( kdmversionum, p_abort ); if ( kdmversionum != 0 ) return 0;
	fil->read_lendian_t( numnotes, p_abort );
	fil->read_lendian_t( numtracks, p_abort );
	fil->read( trinst, numtracks, p_abort );
	fil->read( trquant, numtracks, p_abort );
	fil->read( trvol1, numtracks, p_abort );
	fil->read( trvol2, numtracks, p_abort );
	for ( unsigned i = 0; i < numnotes; i++ ) fil->read_lendian_t( nttime[i], p_abort );
	fil->read( nttrack, numnotes, p_abort );
	fil->read( ntfreq, numnotes, p_abort );
	fil->read( ntvol1, numnotes, p_abort );
	fil->read( ntvol2, numnotes, p_abort );
	fil->read( ntfrqeff, numnotes, p_abort );
	fil->read( ntvoleff, numnotes, p_abort );
	fil->read( ntpaneff, numnotes, p_abort );

	loopcnt = 0;

	return ( scale( nttime[ numnotes - 1 ] - nttime[ 0 ], 1000, 120 ) );
}

long kdmeng::rendersound( void * dasnd, long numbytes )
{
	long i, j, k, voloffs1, voloffs2, *voloffs3, *stempptr;
	long daswave, dasinc, dacnt, bytespertic, numsamplestoprocess;
	unsigned char *sndptr;

	sndptr = ( unsigned char * ) dasnd;

	numsamplestoprocess = ( numbytes >> ( kdmnumspeakers + kdmbytespersample - 2 ) );
	bytespertic = ( kdmsamplerate / 120 );
	for( dacnt = 0; dacnt < numsamplestoprocess; dacnt += bytespertic )
	{
		if ( musicstatus > 0 )    //Gets here 120 times/second
		{
			while ( ( notecnt < numnotes ) && ( timecount >= nttime[ notecnt ] ) )
			{
				j = trinst[ nttrack[ notecnt ] ];
				k = mulscale24( frqtable[ ntfreq[ notecnt ] ], finetune[ j ] + 748 );

				if ( ntvol1[ notecnt ] == 0 )   //Note off
				{
					for( i = NUMCHANNELS - 1 ; i >= 0; i-- )
						if ( splc[ i ] < 0 )
							if ( swavenum[ i ] == j )
								if ( sinc[ i ] == k )
									splc[ i ] = 0;
				}
				else                        //Note on
					startwave( j, k, ntvol1[ notecnt ], ntvol2[ notecnt ], ntfrqeff[ notecnt ], ntvoleff[ notecnt ], ntpaneff[ notecnt ] );

				notecnt++;
				if ( notecnt >= numnotes )
				{
					loopcnt++;
					if ( musicrepeat > 0 )
						notecnt = 0, timecount = nttime[ 0 ];
				}
			}
			timecount++;
		}

		for( i = NUMCHANNELS - 1; i >= 0; i-- )
		{
			if ( splc[ i ] >= 0 ) continue;

			dasinc = sinc[ i ];

			if ( frqeff[ i ] != 0 )
			{
				dasinc = mulscale16( dasinc, eff[ frqeff[ i ] - 1 ][ frqoff[ i ] ] );
				frqoff[ i ]++; if ( frqoff[ i ] >= 256 ) frqeff[ i ] = 0;
			}
			if ( ( voleff[ i ] ) || ( paneff[ i ] ) )
			{
				voloffs1 = svol1[ i ];
				voloffs2 = svol2[ i ];
				if ( voleff[ i ] )
				{
					voloffs1 = mulscale16( voloffs1, eff[ voleff[ i ] - 1 ][ voloff[ i ] ] );
					voloffs2 = mulscale16( voloffs2, eff[ voleff[ i ] - 1 ][ voloff[ i ] ] );
					voloff[ i ]++; if ( voloff[ i ] >= 256 ) voleff[ i ] = 0;
				}

				if ( kdmnumspeakers == 1 )
					calcvolookupmono( &volookup[ i << 9 ], -( voloffs1 + voloffs2 ) << 6, ( voloffs1 + voloffs2 ) >> 1 );
				else
				{
					if ( paneff[ i ] )
					{
						voloffs1 = mulscale16( voloffs1, 131072 - eff[ paneff[ i ] - 1 ][ panoff[ i ] ] );
						voloffs2 = mulscale16( voloffs2, eff[ paneff[ i ] - 1 ][ panoff[ i ] ] );
						panoff[ i ]++; if ( panoff[ i ] >= 256 ) paneff[ i ] = 0;
					}
					calcvolookupstereo( &volookup[ i << 9 ], -( voloffs1 << 7 ), voloffs1, -( voloffs2 << 7 ), voloffs2 );
				}
			}

			daswave = swavenum[ i ];
			voloffs3 = &volookup[ i << 9 ];

			kdmasm1 = repleng[ daswave ];
			kdmasm2 = wavoffs[ daswave ] + repstart[ daswave ] + repleng[ daswave ];
			kdmasm3 = ( repleng[ daswave ] << 12 ); //repsplcoff
			kdmasm4 = soff[ i ];
			if ( kdmnumspeakers == 1 )
				{ splc[ i ] = monohicomb( voloffs3, bytespertic, dasinc, splc[ i ], stemp ); }
			else
				{ splc[ i ] = stereohicomb( voloffs3, bytespertic, dasinc, splc[ i ], stemp ); }
			soff[ i ] = kdmasm4;

			if ( splc[ i ] >= 0 ) continue;
			if ( kdmnumspeakers == 1 )
				{ monohicomb( voloffs3, kdmsamplerate >> 11, dasinc, splc[ i ], &stemp[ bytespertic ] ); }
			else
				{ stereohicomb( voloffs3, kdmsamplerate >> 11, dasinc, splc[ i ], &stemp[ bytespertic << 1 ] ); }
		}

		if ( kdmnumspeakers == 1 )
		{
			for( i = ( kdmsamplerate >> 11 ) - 1; i >= 0; i-- )
				stemp[ i ] += mulscale16( stemp[ i + 1024 ] - stemp[ i ], ramplookup[ i ] );
			j = bytespertic; k = ( kdmsamplerate >> 11 );
			copybuf( ( void * ) &stemp[ j ], ( void * ) &stemp[ 1024 ], k );
			clearbuf( ( void * ) &stemp[ j ], k, 32768 );
		}
		else
		{
			for( i = ( kdmsamplerate >> 11 ) - 1; i >= 0; i-- )
			{
				j = ( i << 1 );
				stemp[ j + 0 ] += mulscale16( stemp[ j + 1024 ] - stemp[ j + 0 ], ramplookup[ i ] );
				stemp[ j + 1 ] += mulscale16( stemp[ j + 1025 ] - stemp[ j + 1 ], ramplookup[ i ] );
			}
			j = ( bytespertic << 1 ); k = ( ( kdmsamplerate >> 11 ) << 1 );
			copybuf( ( void * ) &stemp[ j ], ( void * ) &stemp[ 1024 ], k );
			clearbuf( ( void * ) &stemp[ j ], k, 32768 );
		}

		if ( kdmnumspeakers == 1 )
		{
			if ( kdmbytespersample == 1 ) bound2char( bytespertic >> 1, stemp, sndptr + dacnt );
			else bound2short( bytespertic >> 1, stemp, sndptr + ( dacnt << 1 ) );
		}
		else
		{
			if (kdmbytespersample == 1) bound2char( bytespertic, stemp, sndptr + ( dacnt << 1 ) );
			else bound2short( bytespertic, stemp, sndptr + ( dacnt << 2 ) );
		}
	}
	return(loopcnt);
}

void kdmeng::seek( long seektoms )
{
	long i;

	for( i = 0; i < NUMCHANNELS; i++ ) splc[ i ] = 0;

	i = scale( seektoms, 120, 1000 ) + nttime[0];

	notecnt = 0;
	while ( ( nttime[ notecnt ] < i ) && ( notecnt < numnotes ) ) notecnt++;
	if ( notecnt >= numnotes ) notecnt = 0;

	timecount = nttime[ notecnt ]; loopcnt = 0;
}
