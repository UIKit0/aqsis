// Aqsis
// Copyright � 1997 - 2001, Paul C. Gregory
//
// Contact: pgregory@aqsis.com
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


/** \file
		\brief Implements the classes and support structures for handling polygons.
		\author Paul C. Gregory (pgregory@aqsis.com)
*/

#include	"aqsis.h"
#include	"polygon.h"
#include	"patch.h"
#include	"imagebuffer.h"

START_NAMESPACE( Aqsis )


//---------------------------------------------------------------------
/** Return the boundary extents in camera space of the polygon
 */

CqBound CqPolygonBase::Bound() const
{
	CqVector3D	vecA( FLT_MAX, FLT_MAX, FLT_MAX );
	CqVector3D	vecB( -FLT_MAX, -FLT_MAX, -FLT_MAX );
	TqInt i, n;
	n = NumVertices();
	for ( i = 0; i < n; i++ )
	{
		CqVector3D	vecV = PolyP( i );
		if ( vecV.x() < vecA.x() ) vecA.x( vecV.x() );
		if ( vecV.y() < vecA.y() ) vecA.y( vecV.y() );
		if ( vecV.x() > vecB.x() ) vecB.x( vecV.x() );
		if ( vecV.y() > vecB.y() ) vecB.y( vecV.y() );
		if ( vecV.z() < vecA.z() ) vecA.z( vecV.z() );
		if ( vecV.z() > vecB.z() ) vecB.z( vecV.z() );
	}
	CqBound	B;
	B.vecMin() = vecA;
	B.vecMax() = vecB;
	return ( B );
}


//---------------------------------------------------------------------
/** Split the polygon into bilinear patches.
 *  This split has a special way of dealing with triangles. To avoid the problem of grids with degenerate micro polygons
 *  ( micro polygons with two points the same. ) we treat a triangle as a parallelogram, by extending the fourth point out.
 *  Then everything on the left side of the vector between points 1 and 2 ( on the same side as point 0 ) is rendered, 
 *  everything on the right (the same side as point 3) is not.
 */

TqInt CqPolygonBase::Split( std::vector<CqBasicSurface*>& aSplits )
{
	CqVector3D	vecN;
	TqInt indexA, indexB, indexC, indexD;

	// We need to take into account Orientation here, even though most other
	// primitives leave it up to the CalcNormals function on the MPGrid, because we
	// are forcing N to be setup here, so clockwise nature is important.
	TqInt O = pAttributes() ->GetIntegerAttribute( "System", "Orientation" ) [ 0 ];

	indexA = 0;
	indexB = 1;

	TqInt iUses = PolyUses();
	TqInt n = NumVertices();

	// Get the normals, or calculate the facet normal if not specified.
	if ( !bHasN() )
	{
		CqVector3D vecA = PolyP( indexA );
		// Find two suitable vectors, and produce a geometric normal to use.
		TqInt i = 1;
		CqVector3D	vecN0, vecN1;

		while ( i < n )
		{
			vecN0 = static_cast<CqVector3D>( PolyP( i ) ) - vecA;
			if ( vecN0.Magnitude() > FLT_EPSILON )
				break;
			i++;
		}
		i++;
		while ( i < n )
		{
			vecN1 = static_cast<CqVector3D>( PolyP( i ) ) - vecA;
			if ( vecN1.Magnitude() > FLT_EPSILON && vecN1 != vecN0 )
				break;
			i++;
		}
		vecN = vecN0 % vecN1;
		vecN = ( O == OrientationLH ) ? vecN : -vecN;
		vecN.Unit();
	}

	// Start by splitting the polygon into 4 point patches.
	// Get the normals, or calculate the facet normal if not specified.

	TqInt cNew = 0;
	TqInt i;
	for ( i = 2; i < n; i += 2 )
	{
		indexC = indexD = i;
		if ( n > i + 1 )
			indexD = i + 1;

		// Create bilinear patches
		CqSurfacePatchBilinear* pNew = new CqSurfacePatchBilinear();
		pNew->AddRef();
		pNew->SetSurfaceParameters( Surface() );

		TqInt iUPA = PolyIndex( indexA );
		TqInt iUPB = PolyIndex( indexB );
		TqInt iUPC = PolyIndex( indexC );
		TqInt iUPD = PolyIndex( indexD );

		// Copy any user specified primitive variables.
		std::vector<CqParameter*>::iterator iUP;
		std::vector<CqParameter*>::iterator end = Surface().aUserParams().end();
		for ( iUP = Surface().aUserParams().begin(); iUP != end; iUP++ )
		{
			CqParameter* pNewUP = ( *iUP ) ->CloneType( ( *iUP ) ->strName().c_str(), ( *iUP ) ->Count() );

			if ( pNewUP->Class() == class_varying || pNewUP->Class() == class_vertex )
			{
				pNewUP->SetSize( pNew->cVarying() );
				pNewUP->SetValue( ( *iUP ), 0, iUPA );
				pNewUP->SetValue( ( *iUP ), 1, iUPB );
				pNewUP->SetValue( ( *iUP ), 2, iUPD );
				pNewUP->SetValue( ( *iUP ), 3, iUPC );
				if ( indexC == indexD )
					CreatePhantomData( pNewUP );
			}
			else if ( pNewUP->Class() == class_uniform )
			{
				pNewUP->SetSize( pNew->cUniform() );
				pNewUP->SetValue( ( *iUP ), 0, MeshIndex() );
			}
			else if ( pNewUP->Class() == class_constant )
			{
				pNewUP->SetSize( 1 );
				pNewUP->SetValue( ( *iUP ), 0, 0 );
			}

			pNew->AddPrimitiveVariable( pNewUP );
		}

		// If this is a triangle, then mark the patch as a special case. See function header comment for more details.
		if ( indexC == indexD ) pNew->SetfHasPhantomFourthVertex( TqTrue );

		// If there are no smooth normals specified, then fill in the facet normal at each vertex.
		if ( !bHasN() && USES( iUses, EnvVars_N ) )
		{
			CqParameterTypedVarying<CqVector3D, type_normal, CqVector3D>* pNewUP = new CqParameterTypedVarying<CqVector3D, type_normal, CqVector3D>( "N", 0 );
			pNewUP->SetSize( pNew->cVarying() );

			pNewUP->pValue() [ 0 ] = vecN;
			pNewUP->pValue() [ 1 ] = vecN;
			pNewUP->pValue() [ 2 ] = vecN;
			pNewUP->pValue() [ 3 ] = vecN;

			pNew->AddPrimitiveVariable( pNewUP );
		}

		// If the shader needs s/t or u/v, and s/t is not specified, then at this point store the object space x,y coordinates.
		if ( USES( iUses, EnvVars_s ) || USES( iUses, EnvVars_t ) || USES( iUses, EnvVars_u ) || USES( iUses, EnvVars_v ) )
		{
			CqVector3D PA, PB, PC, PD;
			CqMatrix matID;
			const CqMatrix& matCurrentToWorld = QGetRenderContext() ->matSpaceToSpace( "current", "object", matID, Surface().pTransform() ->matObjectToWorld() );
			PA = matCurrentToWorld * pNew->P() ->pValue() [ 0 ];
			PB = matCurrentToWorld * pNew->P() ->pValue() [ 1 ];
			PC = matCurrentToWorld * pNew->P() ->pValue() [ 3 ];
			PD = matCurrentToWorld * pNew->P() ->pValue() [ 2 ];

			if ( USES( iUses, EnvVars_s ) && !bHass() )
			{
				CqParameterTypedVarying<TqFloat, type_float, TqFloat>* pNewUP = new CqParameterTypedVarying<TqFloat, type_float, TqFloat>( "s" );
				pNewUP->SetSize( pNew->cVarying() );

				pNewUP->pValue() [ 0 ] = PA.x();
				pNewUP->pValue() [ 1 ] = PB.x();
				pNewUP->pValue() [ 2 ] = PD.x();
				pNewUP->pValue() [ 3 ] = PC.x();

				pNew->AddPrimitiveVariable( pNewUP );
			}

			if ( USES( iUses, EnvVars_t ) && !bHast() )
			{
				CqParameterTypedVarying<TqFloat, type_float, TqFloat>* pNewUP = new CqParameterTypedVarying<TqFloat, type_float, TqFloat>( "t" );
				pNewUP->SetSize( pNew->cVarying() );

				pNewUP->pValue() [ 0 ] = PA.y();
				pNewUP->pValue() [ 1 ] = PB.y();
				pNewUP->pValue() [ 2 ] = PD.y();
				pNewUP->pValue() [ 3 ] = PC.y();

				pNew->AddPrimitiveVariable( pNewUP );
			}

			if ( USES( iUses, EnvVars_u ) && !bHasu() )
			{
				CqParameterTypedVarying<TqFloat, type_float, TqFloat>* pNewUP = new CqParameterTypedVarying<TqFloat, type_float, TqFloat>( "u" );
				pNewUP->SetSize( pNew->cVarying() );

				pNewUP->pValue() [ 0 ] = PA.x();
				pNewUP->pValue() [ 1 ] = PB.x();
				pNewUP->pValue() [ 2 ] = PD.x();
				pNewUP->pValue() [ 3 ] = PC.x();

				pNew->AddPrimitiveVariable( pNewUP );
			}

			if ( USES( iUses, EnvVars_v ) && !bHasv() )
			{
				CqParameterTypedVarying<TqFloat, type_float, TqFloat>* pNewUP = new CqParameterTypedVarying<TqFloat, type_float, TqFloat>( "v" );
				pNewUP->SetSize( pNew->cVarying() );

				pNewUP->pValue() [ 0 ] = PA.y();
				pNewUP->pValue() [ 1 ] = PB.y();
				pNewUP->pValue() [ 2 ] = PD.y();
				pNewUP->pValue() [ 3 ] = PC.y();

				pNew->AddPrimitiveVariable( pNewUP );
			}

		}

		aSplits.push_back( pNew );
		cNew++;

		// Move onto the next quad
		indexB = indexD;
	}
	return ( cNew );
}


//---------------------------------------------------------------------
/** Generate phanton data to 'stretch' the triangle patch into a parallelogram.
 */

void CqPolygonBase::CreatePhantomData( CqParameter* pParam )
{
	assert( pParam->Class() == class_varying || pParam->Class() == class_vertex );

	switch ( pParam->Type() )
	{
			case type_point:
			case type_vector:
			case type_normal:
			{
				CqParameterTyped<CqVector3D, CqVector3D>* pTParam = static_cast<CqParameterTyped<CqVector3D, CqVector3D>*>( pParam );
				pTParam->pValue( 3 ) [ 0 ] = ( pTParam->pValue( 1 ) [ 0 ] - pTParam->pValue( 0 ) [ 0 ] ) + pTParam->pValue( 2 ) [ 0 ];
				break;
			}

			case type_hpoint:
			{
				CqParameterTyped<CqVector4D, CqVector3D>* pTParam = static_cast<CqParameterTyped<CqVector4D, CqVector3D>*>( pParam );
				pTParam->pValue( 3 ) [ 0 ] = ( pTParam->pValue( 1 ) [ 0 ] - pTParam->pValue( 0 ) [ 0 ] ) + pTParam->pValue( 2 ) [ 0 ];
				break;
			}

			case type_float:
			{
				CqParameterTyped<TqFloat, TqFloat>* pTParam = static_cast<CqParameterTyped<TqFloat, TqFloat>*>( pParam );
				pTParam->pValue( 3 ) [ 0 ] = ( pTParam->pValue( 1 ) [ 0 ] - pTParam->pValue( 0 ) [ 0 ] ) + pTParam->pValue( 2 ) [ 0 ];
				break;
			}

			case type_integer:
			{
				CqParameterTyped<TqInt, TqFloat>* pTParam = static_cast<CqParameterTyped<TqInt, TqFloat>*>( pParam );
				pTParam->pValue( 3 ) [ 0 ] = ( pTParam->pValue( 1 ) [ 0 ] - pTParam->pValue( 0 ) [ 0 ] ) + pTParam->pValue( 2 ) [ 0 ];
				break;
			}

			case type_color:
			{
				CqParameterTyped<CqColor, CqColor>* pTParam = static_cast<CqParameterTyped<CqColor, CqColor>*>( pParam );
				pTParam->pValue( 3 ) [ 0 ] = ( pTParam->pValue( 1 ) [ 0 ] - pTParam->pValue( 0 ) [ 0 ] ) + pTParam->pValue( 2 ) [ 0 ];
				break;
			}

			case type_matrix:
			{
				CqParameterTyped<CqMatrix, CqMatrix>* pTParam = static_cast<CqParameterTyped<CqMatrix, CqMatrix>*>( pParam );
				pTParam->pValue( 3 ) [ 0 ] = ( pTParam->pValue( 1 ) [ 0 ] - pTParam->pValue( 0 ) [ 0 ] ) + pTParam->pValue( 2 ) [ 0 ];
				break;
			}

			case type_string:
			{
				CqParameterTyped<CqString, CqString>* pTParam = static_cast<CqParameterTyped<CqString, CqString>*>( pParam );
				pTParam->pValue( 3 ) [ 0 ] = ( pTParam->pValue( 1 ) [ 0 ] - pTParam->pValue( 0 ) [ 0 ] ) + pTParam->pValue( 2 ) [ 0 ];
				break;
			}
	}
}

//---------------------------------------------------------------------
/** Default constructor.
 */

CqSurfacePolygon::CqSurfacePolygon( TqInt cVertices ) : CqSurface(),
		m_cVertices( cVertices )
{}


//---------------------------------------------------------------------
/** Copy constructor.
 */

CqSurfacePolygon::CqSurfacePolygon( const CqSurfacePolygon& From )
{
	*this = From;
}


//---------------------------------------------------------------------
/** Destructor.
 */

CqSurfacePolygon::~CqSurfacePolygon()
{}


//---------------------------------------------------------------------
/** Check if a polygon is degenerate, i.e. all points collapse to the same or almost the same place.
 */

TqBool CqSurfacePolygon::CheckDegenerate() const
{
	// Check if all points are within a minute distance of each other.
	TqBool	fDegen = TqTrue;
	TqInt i, n;
	n = NumVertices();
	for ( i = 1; i < n; i++ )
	{
		if ( ( PolyP( i ) - PolyP( i - 1 ) ).Magnitude() > FLT_EPSILON )
			fDegen = TqFalse;
	}
	return ( fDegen );
}

//---------------------------------------------------------------------
/** Assignment operator.
 */

CqSurfacePolygon& CqSurfacePolygon::operator=( const CqSurfacePolygon& From )
{
	CqSurface::operator=( From );
	m_cVertices = From.m_cVertices;
	return ( *this );
}


//---------------------------------------------------------------------
/** Copy constructor.
 */

CqSurfacePointsPolygon::CqSurfacePointsPolygon( const CqSurfacePointsPolygon& From ) : m_pPoints( 0 )
{
	*this = From;
}


//---------------------------------------------------------------------
/** Assignment operator.
 */

CqSurfacePointsPolygon& CqSurfacePointsPolygon::operator=( const CqSurfacePointsPolygon& From )
{
	TqInt i;
	m_aIndices.resize( From.m_aIndices.size() );
	for ( i = From.m_aIndices.size() - 1; i >= 0; i-- )
		m_aIndices[ i ] = From.m_aIndices[ i ];

	// Store the old points array pointer, as we must reference first then
	// unreference to avoid accidental deletion if they are the same and we are the
	// last reference.
	CqPolygonPoints*	pOldPoints = m_pPoints;
	m_pPoints = From.m_pPoints;
	m_Index = From.m_Index;
	m_pPoints->AddRef();
	if ( pOldPoints ) pOldPoints->Release();

	return ( *this );
}




//---------------------------------------------------------------------
/** Transform the points by the specified matrix.
 */

void	CqPolygonPoints::Transform( const CqMatrix& matTx, const CqMatrix& matITTx, const CqMatrix& matRTx, TqInt iTime )
{
	if ( m_Transformed ) return ;

	CqSurface::Transform( matTx, matITTx, matRTx, iTime );

	m_Transformed = TqTrue;
}



//---------------------------------------------------------------------
/** Get the bound of this GPrim.
 */

CqBound	CqMotionSurfacePointsPolygon::Bound() const
{
	CqMotionSurfacePointsPolygon * pthis = const_cast<CqMotionSurfacePointsPolygon*>( this );
	CqBound B( FLT_MAX, FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX );
	TqInt i;
	for ( i = 0; i < cTimes(); i++ )
	{
		pthis->m_CurrTimeIndex = i;
		B.Encapsulate( CqPolygonBase::Bound() );
	}
	return ( AdjustBoundForTransformationMotion( B ) );
}



//---------------------------------------------------------------------
/** Split this GPrim into smaller GPrims.
 */

TqInt CqMotionSurfacePointsPolygon::Split( std::vector<CqBasicSurface*>& aSplits )
{
	std::vector<std::vector<CqBasicSurface*> > aaMotionSplits;
	aaMotionSplits.resize( cTimes() );
	TqInt cSplits = 0;
	TqInt i;
	for ( i = 0; i < cTimes(); i++ )
	{
		m_CurrTimeIndex = i;
		cSplits = CqPolygonBase::Split( aaMotionSplits[ i ] );
	}

	// Now build motion surfaces from the splits and pass them back.
	for ( i = 0; i < cSplits; i++ )
	{
		CqMotionSurface<CqBasicSurface*>* pNewMotion = new CqMotionSurface<CqBasicSurface*>( 0 );
		pNewMotion->AddRef();
		pNewMotion->m_fDiceable = TqTrue;
		pNewMotion->m_EyeSplitCount = m_EyeSplitCount;
		TqInt j;
		for ( j = 0; j < cTimes(); j++ )
			pNewMotion->AddTimeSlot( Time( j ), aaMotionSplits[ j ][ i ] );
		aSplits.push_back( pNewMotion );
	}
	return ( cSplits );
}


END_NAMESPACE( Aqsis )
//---------------------------------------------------------------------
