// TImageList.h: interface for the TImageList class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_TIMAGELIST_H__32028F19_B866_4E3C_8120_CB3D8EE18F87__INCLUDED_)
#define AFX_TIMAGELIST_H__32028F19_B866_4E3C_8120_CB3D8EE18F87__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// =====================================================================
/**	@class		TImageList
	@brief		º¹¼öÀÇ ÀÌ¹ÌÁö¸¦ À¯ÁöÇÏ¸ç ÇÑ¹ø¿¡ ÇÏ³ªÀÇ ÀÌ¹ÌÁö¸¦ ¼±ÅÃ ÇÒ 
				¼ö ÀÖ´Â ÄÄÆ÷³ÍÆ®ÀÌ´Ù.
	
*/// ===================================================================
class TImageList : public TComponent  
{

public:
	/// ÀÌ¹ÌÁö¸®½ºÆ®¸¦ ´ëÇ¥ÇÏ´Â Å¬·¡½ºÀÌ´Ù.
	class TDelegate : public TComponent
	{
	public:
		/// ÁÖ¾îÁø ÀÌ¹ÌÁö¸®½ºÆ®ÀÇ CurIdx¸¦ ÂüÁ¶ÇÏ¿© ·»´õÇÑ´Ù.
		TComponent* RenderImgList( TImageList* pImgList, DWORD dwCurTick, DWORD dwDeltaTick);
		/// ÁÖ¾îÁø ÀÌ¹ÌÁö(pImgComp)¸¦ ÂüÁ¶ÇÏ¿© ·»´õÇÑ´Ù.
		void RenderImgComp( TImageList* pImgList, TComponent* pImgComp, DWORD dwCurTick, DWORD dwDeltaTick);
		/// ÁÖ¾îÁø ÀÌ¹ÌÁö¸®½ºÆ®¸¦ ÂüÁ¶ÇÏ¿© ¿µ¿ª°Ë»ç¸¦ ¼öÇàÇÑ´Ù.
		BOOL HitRectDelegate( TImageList* pImgList, CPoint pt);
		/// ÁÖ¾îÁø ÀÌ¹ÌÁö¸®½ºÆ®¸¦ ÂüÁ¶ÇÏ¿© Ãæµ¹°Ë»ç¸¦ ¼öÇàÇÑ´Ù.
		BOOL HitTestDelegate( TImageList* pImgList, CPoint pt);
		/// ÀÌ¹ÌÁöÀÇ ÀüÃ¼ °¹¼ö¸¦ ¾ò´Â´Ù.
		int GetImageCount() const;
		/// ÀÌ¹ÌÁö ¾ò±â
		TComponent* GetImage( int nIndex ) const;

		virtual TComponent* Clone() const
		{
			return nullptr;
		};

	public:
		TDelegate(FRAMEDESC_SHAREDPTR pDesc);
		~TDelegate();
	};

	struct tFrameCmp
	{
		bool operator() (FRAMEDESC_SHAREDPTR lhs, FRAMEDESC_SHAREDPTR rhs) const;
	};

public:
	typedef std::shared_ptr<TDelegate> DELEGATE_SHAREDPTR;

private:
	typedef std::map<FRAMEDESC_SHAREDPTR, DELEGATE_SHAREDPTR, tFrameCmp> TDELEGATEMAP;

private:
	static TDELEGATEMAP s_mapDELEGATE;

	static TImageList::DELEGATE_SHAREDPTR CreateDelegate(FRAMEDESC_SHAREDPTR rpDesc);
	static void ReleaseDelegate(const TImageList::DELEGATE_SHAREDPTR& rpDelegate);

public:
	static void ClearDelegates();

protected:
	int			m_nCurIdx;		///< ÇöÀç ¼±ÅÃµÈ ÀÌ¹ÌÁöÀÇ ÀÎµ¦½º
	int			m_nLastIdx;		///< °¡Àå ÃÖ±Ù¿¡ ±×·ÁÁø ÀÌ¹ÌÁöÀÇ ÀÎµ¦½º
	TComponent*	m_pLastImg;		///< °¡Àå ÃÖ±Ù¿¡ ±×·ÁÁø ÀÌ¹ÌÁö
	TComponent*	m_pSkinImg;		///< ½ºÅ²
	
	BOOL		m_bUserColor;	///< À¯Àú°¡ Á¤ÇÑ »öÀ» »ç¿ëÇÒÁö ¿©ºÎ
	DWORD		m_dwUserColor;	///< À¯Àú°¡ Á¤ÇÑ »ö
	BOOL		m_bUsePixelHitTest; ///< ÇÈ¼¿´ÜÀ§·Î HitTest °Ë»ç ¿©ºÎ

	DELEGATE_SHAREDPTR m_pDelegate;

protected:
	virtual TComponent* Clone() const
	{
		return new TImageList(m_pParent, *this);
	};
	
public:
	virtual BOOL HitRect( CPoint pt);
	virtual BOOL HitTest( CPoint pt);
	virtual HRESULT Render( DWORD dwTickCount);
	
public:
	BOOL EndOfImgs( TCOMP_LIST::iterator it );
	TCOMP_LIST::iterator GetFirstImgsFinder();
	TComponent* GetNextImg( TCOMP_LIST::iterator &it );

protected:
	void InitRect();

public:
	void SetCurImage(int nIndex)
	{ 
		m_nCurIdx = nIndex; 
	};

	int GetCurImage() const
	{ 
		return m_nCurIdx;
	};

	void EnableUserColor(BOOL bEnable)	
	{ 
		m_bUserColor = bEnable;
	};

	BOOL IsUserColorEnabled() const
	{ 
		return m_bUserColor; 
	};

	void SetUserColor(DWORD dwColor)
	{
		m_dwUserColor = dwColor; 
	};

	DWORD GetUserColor() const
	{ 
		return m_dwUserColor; 
	};

	int GetImageCount()	const 
	{ 
		ASSERT(m_pDelegate);
		return m_pDelegate->GetImageCount(); 
	};

	const DELEGATE_SHAREDPTR& GetDelegate() const		
	{
		return m_pDelegate; 
	};

	void SetPixelHitTest(BOOL bUse)	
	{ 
		m_bUsePixelHitTest = bUse; 
	};

	void SetSkinImage( int nIndex );
	void SetSkinImageEmpty();

	void UpdateLastImg() 
	{ 
		m_nLastIdx = -1; 
	};

public:
	TImageList(TComponent* pParent, FRAMEDESC_SHAREDPTR pDesc);
	TImageList(TComponent* pParent, FRAMEDESC_WEAKPTR pDesc);
	TImageList(TComponent* pParent, const TImageList& rSrcImgLst);
	virtual ~TImageList();
};

#endif // !defined(AFX_TIMAGELIST_H__32028F19_B866_4E3C_8120_CB3D8EE18F87__INCLUDED_)



















