/*----------------------------------------------------------
    Charu3Treeクラスヘッダ
                                    2002/11/16 (c)Keizi
----------------------------------------------------------*/

#if !defined(AFX_CHARU3TREE_H__246081ED_82BD_4EFE_AA98_067AC36E58E6__INCLUDED_)
#define AFX_CHARU3TREE_H__246081ED_82BD_4EFE_AA98_067AC36E58E6__INCLUDED_

// TODO: Consistent naming

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "stdafx.h"

#include <list>
#include <vector>

namespace dataTree {
    const int ROOT = -1;
}

//データ種別
#define KIND_ONETIME 0x01	//ワンタイム項目
#define KIND_LOCK    0x02	//ノーマル項目
#define KIND_FOLDER  0x04	//フォルダ項目
#define KIND_RIREKI  0x08	//履歴フォルダ

#define KIND_DATA_ALL	 0x03	//データ
#define KIND_FOLDER_ALL  0x0C	//フォルダ

#define FOLDER_OPEN 0x10	//フォルダオープンマスク

//データアイコン種別
#define KIND_DEFAULT 0  //アイコン指定無し

#define KIND_DATE	1	//日付
#define KIND_EXE    2	//実行
#define KIND_RELATE 3	//関連付け
#define KIND_SELECT 4	//選択
#define KIND_CLIP   5	//クリップボード
#define KIND_PLUG   6   //プラグイン
#define KIND_KEY    7   //キーマクロ

//---------------------------------------------------
// プラグイン構造体
//---------------------------------------------------
struct RW_PLUGIN
{
    CString m_strPluginFilePath; // Pathname of the plugin
    CString m_strFormatName; // Data format name
    CString m_strExtension; // Extension of data file

    RW_PLUGIN(CString path, TCHAR* format, TCHAR* ext)
        : m_strPluginFilePath(CString(path))
        , m_strFormatName(CString(format))
        , m_strExtension(CString(ext))
    {
    }

    RW_PLUGIN()
    {
    }
};

//---------------------------------------------------
// データ構造体
//---------------------------------------------------
struct STRING_DATA
{
    char	m_cKind;		//データ種別
    char	m_cIcon;		//アイコン種別
    int		m_nMyID;		//データのID
    int		m_nParentID;	//親データのID
    time_t	m_timeCreate;	//作成日時 32bit
    time_t	m_timeEdit;		//変更日時 32bit
    CString  m_strTitle;	//設定データタイトル
    CString  m_strData;		//設定データ文字列
    CString  m_strMacro;	//拡張用文字列データ

    STRING_DATA()
        : m_cKind(KIND_LOCK)
        , m_cIcon(0)
        , m_nMyID(0)
        , m_nParentID(0)
        , m_timeCreate(0)
        , m_timeEdit(0)
        , m_strTitle(_T(""))
        , m_strData(_T(""))
        , m_strMacro(_T(""))
    {
    }
};

typedef bool (*writeDataFile)(CString strFileName, std::list<STRING_DATA>* pList);
typedef std::list<STRING_DATA>* (*readDataFile)(CString strFileName, bool isImport);
typedef bool (*convertMacro)(TCHAR* szSource, TCHAR* szRet, int nSize, TCHAR* szSelect, TCHAR* szClip);
typedef void (*getExtension)(TCHAR* szBuff, int nSize);
typedef void (*getFormatName)(TCHAR* szBuff, int nSize);
typedef void (*endDLL)();
typedef void (*initDLL)();
typedef bool (*isUnicode)();

//---------------------------------------------------
// CCharu3Tree ウィンドウ
//---------------------------------------------------
class CCharu3Tree : public CTreeCtrl
{
    // コンストラクション
public:
    CCharu3Tree();
    virtual ~CCharu3Tree();

    // オーバーライド
    // ClassWizard は仮想関数のオーバーライドを生成します。
    //{{AFX_VIRTUAL(CCharu3Tree)
    virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
    //}}AFX_VIRTUAL

    // インプリメンテーション
public:
    /**
     * @brief Add a folder with the specified name.
     * @param hTreeItem The insert position (inserted after this). If nullptr, it will be added to the top of the tree.
     * @param strName The folder name.
     * @return The added tree item.
     */
    HTREEITEM AddFolder(HTREEITEM hTreeItem, CString strName);
 
    /**
     * @brief Add a specified data.
     * @param hTreeItem The insert position. If nullptr, it will be added to the top of the tree.
     * @param data The data to add.
     * @param asChild If true, insert as the first child of hTreeItem, if false, insert after hTreeItem.
     * @return The added tree item.
     */
    HTREEITEM AddData(HTREEITEM hTreeItem, STRING_DATA data, bool asChild = false);

    /**
     * @brief Add a specified data to the history folders.
     * @param data The data to add.
     */
    void AddDataToHistoryFolders(STRING_DATA data);

    /**
     * @brief Delete a specified data.
     * @param hTreeItem The tree item to delete.
     */
    void DeleteData(HTREEITEM hTreeItem);

    /**
     * @brief Delete children of a specified item (clear contents of folder).
     * @param hTreeItem The parent tree item (folder).
     */
    void DeleteChildren(HTREEITEM hTreeItem);

    /**
     * @brief Remove an specified item from the tree control.
     * @param hTreeItem The tree item to delete.
     */
    void DeleteItem(HTREEITEM hTreeItem);

    HTREEITEM GetTrueNextItem(HTREEITEM hTreeItem);
    HTREEITEM GetTruePrevItem(HTREEITEM hTreeItem);
    HTREEITEM GetLastVisibleItem();
    HTREEITEM GetLastSiblingItem(HTREEITEM hTreeItem);
    HTREEITEM GetLastDescendantItem(HTREEITEM hTreeItem);

    void UpdateItem(HTREEITEM hTreeItem);
    void SetText(HTREEITEM hTreeItem, CString& strText);

    void setPlugin(CString strPath);
    void setImageList(POINT posSize, CString strFileName, CString strPath);

    void closeFolder(HTREEITEM hStartItem);
    void cleanupOneTimeItems(HTREEITEM hStartItem, int nKind = 0);
    void changeIcon(HTREEITEM hTreeItem, int nID);
    void tree2List(HTREEITEM hStartItem, std::list<STRING_DATA>* tmplist, bool isAll = false);

    STRING_DATA getData(HTREEITEM hTreeItem);
    STRING_DATA* getDataPtr(HTREEITEM hTreeItem);

    bool convertMacroPlugin(const STRING_DATA* sourceDataPtr, CString* strRet, CString strSelect, CString strClip, CString strSoftName);

    HTREEITEM searchItem(HTREEITEM hStartItem, bool backward = false);
    HTREEITEM searchTitle(HTREEITEM hStartItem, CString strKey, bool makeLower = false);

    void copyChildren(HTREEITEM hFromItem, HTREEITEM hToItem);

    char getDatakind(HTREEITEM hTreeItem) {
        return hTreeItem ? getDataPtr(hTreeItem)->m_cKind : 0;
    }

    void copyData(int nParentID, HTREEITEM hParentTreeItem, std::list<STRING_DATA>* pList);
    HTREEITEM mergeTreeData(HTREEITEM hTreeItem, std::list<STRING_DATA>* pList, bool isRoot);

    bool loadDataFileDef(CString strFileName, CString strPlugin);
    bool loadDataFile(CString strFileName, CString strPlugin, std::list<STRING_DATA>* tmplist);
    bool saveDataToFile(CString strFileName, CString strPlugin, HTREEITEM hStartItem);

    void setInitInfo(int* pMaxID, int* pSelectID, int* pRecNumber) {
        m_pMaxID = pMaxID;
        m_pSelectID = pSelectID;
        m_pRecNumber = pRecNumber;
    }

    bool checkMyChild(HTREEITEM hMeItem, HTREEITEM hChildItem);
    HTREEITEM searchMyRoots(HTREEITEM hStartItem);
    HTREEITEM searchParentOption(HTREEITEM hStartItem, CString strOption);

    void allIconCheck();
    char decideIcon(CString strData);
    CString makeTitle(CString strData, int nTitleLength = 32);
    HTREEITEM getOneTimeItem(int nType);

    int getDataOption(CString strData, CString strKind);
    CString getDataOptionStr(CString strData, CString strKind);
    HTREEITEM moveFolderTop(HTREEITEM hTreeItem);
    void ToggleItemCheck(HTREEITEM hItem);

    void setScrollBar();
    bool IsDragging() { return m_dragState != DragState::NOT_DRAGGING; }

    std::list<STRING_DATA> m_MyStringList;//データリスト
    std::vector<RW_PLUGIN>  m_rwPlugin;
    std::list<HTREEITEM> m_ltCheckItems;

private:
    std::list<STRING_DATA>::iterator findData(STRING_DATA* dataPtr);
    int mergeList(std::list<STRING_DATA>* pMainList, std::list<STRING_DATA>* pList, int nParent);

    int makeNewID();
    void setSelectID(int nID) { *m_pSelectID = nID; }
    bool checkRedundancyID(int nID);
    void normalizationID(std::list<STRING_DATA>* pList, int nParentID);

    int getChildCount(HTREEITEM hTreeItem, bool isBrotherOnly = false);
    HTREEITEM getFirstFolder(HTREEITEM hStartItem);
    HTREEITEM getLastChild(HTREEITEM hStartItem);

    void moveChildren(HTREEITEM hFromItem, HTREEITEM hToItem);
    void deleteExcessChildren(HTREEITEM hTreeItem, int* nCount);

    bool hasDataOption(CString strData, CString strKind);
    DWORD getDataOptionHex(CString strData, CString strKind);
    void archiveHistory(HTREEITEM hTreeItem, int nRirekiCount);

    void SetCheckEx(HTREEITEM hItem, BOOL check);
    void SetCheckExRecursive(HTREEITEM hStartItem, BOOL check);
    void UncheckItem(HTREEITEM hItem);

    bool getPlugin(CString strName, RW_PLUGIN* pPlugin);
    bool LoadDataWithPlugin(CString strFileName, CString strPlugin, std::list<STRING_DATA>* tmplist);
    bool SaveDataWithPlugin(CString strFileName, CString strPlugin, std::list<STRING_DATA>* tmplist);

    enum class DragState {
        NOT_DRAGGING,
        INSERT_BEFORE,
        PLACE_INSIDE,
        INSERT_AFTER
    };
    DragState m_dragState;
    ULONGLONG m_nHoverTick;
    HTREEITEM m_hDragItem;
    HTREEITEM m_hDragTarget;
    HTREEITEM m_hPrevTarget;
    CImageList* m_pDragImage;

    int* m_pMaxID;
    int* m_pSelectID;
    int* m_pRecNumber;
    CString m_strPreviousRecordedToHistory;

    CImageList* m_ImageList;//イメージリスト

    // 生成されたメッセージ マップ関数
public:
    //{{AFX_MSG(CCharu3Tree)
    afx_msg void OnWindowPosChanging(WINDOWPOS FAR* lpwndpos);
protected:
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnBegindrag(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnItemexpanding(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnSelchanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnItemexpanded(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnNcCalcSize(BOOL bCalcValidRects, NCCALCSIZE_PARAMS* lpncsp);
    //}}AFX_MSG

    DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ は前行の直前に追加の宣言を挿入します。

#endif // !defined(AFX_CHARU3TREE_H__246081ED_82BD_4EFE_AA98_067AC36E58E6__INCLUDED_)
