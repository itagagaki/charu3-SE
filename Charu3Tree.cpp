/*----------------------------------------------------------
    Charu3Treeクラス
                                    2002/11/16 (c)Keizi
----------------------------------------------------------*/

#include "stdafx.h"
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#include <list>
#include <vector>

#include "Charu3Tree.h"
#include "search.h"
#include "text.h"
#include "Charu3.h"
#include "log.h"

#define ICON_FOLDER	 0
#define ICON_RIREKI_FOLDER 2
#define ICON_ONETIME 4
#define ICON_KEY	 6
#define ICON_DATE	 8                                                                      
#define ICON_EXE	10
#define ICON_RELATE	12
#define ICON_SELECT	14
#define ICON_CLIP	16
#define ICON_PLUG	18
#define ICON_KEYM	20

namespace {

    //---------------------------------------------------
    //関数名	getIconNumber(char cKind,char cIcon)
    //機能		アイコン番号を取得
    //---------------------------------------------------
    int getIconNumber(char cKind, char cIcon)
    {
        int nRet = 0;
        if (cKind & KIND_FOLDER) {
            nRet = ICON_FOLDER;
        }
        else if (cKind & KIND_RIREKI) {
            nRet = ICON_RIREKI_FOLDER;
        }
        else if (cKind & KIND_LOCK) {//ロック項目の場合
            if (cIcon > 0) {
                int nIcon = cIcon * 2;
                nRet = ICON_KEY + nIcon;
            }
            else {
                nRet = ICON_KEY;
            }
        }
        else {//一時項目の場合
            nRet = ICON_ONETIME;
        }
        return nRet;
    }

    //---------------------------------------------------
    //関数名	data2TreeStruct(TV_INSERTSTRUCT& pTreeCtrlItem, list<STRING_DATA>::iterator pit)
    //機能		ツリー構造体にデータを格納
    //---------------------------------------------------
    void data2TreeStruct(TV_INSERTSTRUCT& treeCtrlItem, std::list<STRING_DATA>::iterator it)
    {
        //ツリーデータ作成
        treeCtrlItem.hInsertAfter = TVI_LAST;
        treeCtrlItem.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
        STRING_DATA data;
        data.m_strTitle = it->m_strTitle;

        //アイコンを設定
        int nIcon = getIconNumber(it->m_cKind, it->m_cIcon);
        treeCtrlItem.item.iImage = nIcon;
        treeCtrlItem.item.iSelectedImage = nIcon + 1;

        //名前を設定
        treeCtrlItem.item.pszText = (TCHAR*)LPCTSTR(it->m_strTitle);
        //データのアドレスを設定
        treeCtrlItem.item.lParam = (LPARAM) & *it;
    }

    bool findKeywords(CString text, std::vector<CString> words)
    {
        if (theApp.m_ini.m_bSearchCaseInsensitive) {
            text.MakeLower();
        }
        bool found = (theApp.m_ini.m_nSearchLogic == SEARCH_LOGIC_AND);
        for (std::vector<CString>::iterator it = words.begin(); it != words.end(); it++) {
            bool matched = (text.Find(*it) != -1);
            if (!matched && theApp.m_ini.m_nSearchLogic == SEARCH_LOGIC_AND) {
                found = false;
                break;
            }
            if (matched && theApp.m_ini.m_nSearchLogic == SEARCH_LOGIC_OR) {
                found = true;
                break;
            }
        }
        return found;
    }

    bool loadBitmapFile(CString strFileName, CBitmap* BitMap)
    {
        HBITMAP hBitmap;
        BitMap->Detach();
        hBitmap = (HBITMAP)::LoadImage(NULL, strFileName, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
        if (hBitmap == NULL) {
            return false;
        }
        BitMap->Attach(hBitmap);
        return true;
    }

} // anonymous namespace

//---------------------------------------------------
//関数名	CCharu3Tree
//機能		コンストラクタ
//---------------------------------------------------
CCharu3Tree::CCharu3Tree()
    : m_dragState(DragState::NOT_DRAGGING)
    , m_nHoverTick(0)
    , m_hDragItem(nullptr)
    , m_hDragTarget(nullptr)
    , m_hPrevTarget(nullptr)
    , m_pDragImage(nullptr)
    , m_ImageList(nullptr)
    , m_pMaxID(nullptr)
    , m_pSelectID(nullptr)
    , m_pRecNumber(nullptr)
{
    m_MyStringList.clear();
}

//---------------------------------------------------
//関数名	~CCharu3Tree
//機能		デストラクタ
//---------------------------------------------------
CCharu3Tree::~CCharu3Tree()
{
    if (m_ImageList)	delete m_ImageList;
}

BEGIN_MESSAGE_MAP(CCharu3Tree, CTreeCtrl)
    //{{AFX_MSG_MAP(CCharu3Tree)
    ON_WM_MOUSEMOVE()
    ON_NOTIFY_REFLECT(TVN_BEGINDRAG, OnBegindrag)
    ON_WM_LBUTTONUP()
    ON_NOTIFY_REFLECT(TVN_ITEMEXPANDING, OnItemexpanding)
    ON_NOTIFY_REFLECT(TVN_SELCHANGED, OnSelchanged)
    ON_WM_MOUSEWHEEL()
    ON_NOTIFY_REFLECT(TVN_ITEMEXPANDED, OnItemexpanded)
    ON_WM_WINDOWPOSCHANGING()
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

HTREEITEM CCharu3Tree::AddFolder(HTREEITEM hTreeItem, CString strName)
{
    if (theApp.m_ini.m_bDebug) {
        LOG(_T("AddFolder %s"), strName.GetString());
    }

    STRING_DATA data;
    data.m_cKind = KIND_FOLDER;
    data.m_strTitle = strName;
    HTREEITEM hNewItem = AddData(hTreeItem, data);
    SelectItem(hNewItem);

    return hNewItem;
}

HTREEITEM CCharu3Tree::AddData(HTREEITEM hTreeItem, STRING_DATA data, bool asChild /* = false */)
{
    if (theApp.m_ini.m_bDebug) {
        LOG(_T("AddData %s %s %d"), data.m_strTitle.GetString(), data.m_strData.GetString(), data.m_cKind);
    }

    data.m_nMyID = makeNewID();
    data.m_nParentID = dataTree::ROOT;

    if (hTreeItem) {
        HTREEITEM hParentItem = asChild ? hTreeItem : GetParentItem(hTreeItem);
        if (hParentItem) {
            data.m_nParentID = getDataPtr(hParentItem)->m_nMyID;
        }
    }

    if (data.m_strTitle == "") data.m_strTitle = makeTitle(data.m_strData);

    time(&data.m_timeCreate);
    time(&data.m_timeEdit);
    std::list<STRING_DATA>::iterator it = m_MyStringList.insert(m_MyStringList.end(), data);//リストに追加

    TV_INSERTSTRUCT treeCtrlItem;
    data2TreeStruct(treeCtrlItem, it);

    if (hTreeItem) {
        if (asChild) {
            treeCtrlItem.hParent = hTreeItem;
            treeCtrlItem.hInsertAfter = TVI_FIRST;
        }
        else {
            treeCtrlItem.hParent = GetParentItem(hTreeItem);
            treeCtrlItem.hInsertAfter = hTreeItem;
        }
    }
    else {
        treeCtrlItem.hParent = TVI_ROOT;
        treeCtrlItem.hInsertAfter = TVI_FIRST;
    }

    return InsertItem(&treeCtrlItem);
}

void CCharu3Tree::AddDataToHistoryFolders(STRING_DATA data)
{
    int nSize = m_MyStringList.size(), nRirekiCount, nIsLock, i;
    int nNumber = *m_pRecNumber;

    STRING_DATA* parentData;
    HTREEITEM hTreeItem, hStart = nullptr;

    if (theApp.m_ini.m_bDebug) {
        LOG(_T("addDataToRecordFolder"));
        LOG(_T("hRootItem:%p TreeSize:%d"), GetRootItem(), nSize);
    }
    //	hTreeItem = (HTREEITEM)::SendMessage(m_hWnd, TVM_GETNEXTITEM, TVGN_ROOT, 0);
    for (hTreeItem = GetRootItem(), i = 0; i < nSize && hTreeItem && hTreeItem != hStart; i++, hTreeItem = GetTrueNextItem(hTreeItem)) {
        parentData = getDataPtr(hTreeItem);
        hStart = GetRootItem();
        if ((parentData->m_cKind & KIND_RIREKI)
            && (
                (data.m_strData != m_strPreviousRecordedToHistory)
                || !(getDataOption(parentData->m_strMacro, _T("duplicationcheck")))
                )
            ) {
            if (*m_pRecNumber == nNumber)	nNumber++;

            if (theApp.m_ini.m_bDebug) {
                LOG(_T("number:%d handle:%p title:%s ID:%d"), i, hTreeItem, parentData->m_strTitle.GetString(), parentData->m_nMyID);
            }

            nIsLock = getDataOption(parentData->m_strMacro, _T("lock"));//履歴種別を取得
            if (!nIsLock)	data.m_cKind = KIND_ONETIME;
            else			data.m_cKind = KIND_LOCK;

            nRirekiCount = getDataOption(parentData->m_strMacro, _T("count"));//履歴数を取得
            if (nRirekiCount <= 0) nRirekiCount = 2;
            int nDeleteCount = nRirekiCount;
            deleteExcessChildren(GetChildItem(hTreeItem), &nDeleteCount);//余計な子を消す

            int nTitleLength = getDataOption(parentData->m_strMacro, _T("titlelen"));//タイトルの文字数
            CString strTitle;
            if (nTitleLength > 0 && nTitleLength < 256) {
                strTitle = makeTitle(data.m_strData, nTitleLength);
            }
            else
                strTitle = makeTitle(data.m_strData);
            data.m_strTitle = strTitle;

            if (getDataOption(parentData->m_strMacro, _T("number"))) {//番号をつけるかどうか
                data.m_strTitle.Format(_T("%02d : %s"), *m_pRecNumber, strTitle.GetString());
                if (nNumber > nRirekiCount) nNumber = 0;
            }

            // archiving
            {
                int archiveBy;
                if (hasDataOption(parentData->m_strMacro, _T("archiveunit"))) {
                    archiveBy = getDataOption(parentData->m_strMacro, _T("archiveunit"));
                }
                else {
                    archiveBy = getDataOption(parentData->m_strMacro, _T("classhistory")); // for historical compatibility
                }
                if (archiveBy > 0) {
                    archiveHistory(hTreeItem, archiveBy);
                }
            }

            if (theApp.m_ini.m_bDebug) {
                LOG(_T("AddDataToHistoryFolders %s %s %d"), data.m_strTitle.GetString(), data.m_strData.GetString(), nNumber);
            }

            AddData(hTreeItem, data, true);//追加
            nSize = m_MyStringList.size();
        }
    }
    *m_pRecNumber = nNumber;
    m_strPreviousRecordedToHistory = data.m_strData;
}

void CCharu3Tree::DeleteData(HTREEITEM hTreeItem)
{
    STRING_DATA* dataPtr = getDataPtr(hTreeItem);

    if (theApp.m_ini.m_bDebug) {
        LOG(_T("DeleteData %s %d"), dataPtr->m_strTitle.GetString(), dataPtr->m_cKind);
    }

    DeleteChildren(hTreeItem);
    std::list<STRING_DATA>::iterator it = findData(dataPtr);
    m_MyStringList.erase(it);
    DeleteItem(hTreeItem);
}

void CCharu3Tree::DeleteChildren(HTREEITEM hTreeItem)
{
    HTREEITEM child = GetChildItem(hTreeItem);
    while (child) {
        HTREEITEM next= GetNextItem(child, TVGN_NEXT);
        DeleteData(child);
        child = next;
    }
}

void CCharu3Tree::DeleteItem(HTREEITEM hTreeItem)
{
    UncheckItem(hTreeItem);
    CTreeCtrl::DeleteItem(hTreeItem);
}

//---------------------------------------------------
//関数名	GetTrueNextItem(HTREEITEM hTreeItem)
//機能		次のアイテムを取得(色々処理してます)
//---------------------------------------------------
HTREEITEM CCharu3Tree::GetTrueNextItem(HTREEITEM hTreeItem)
{
    HTREEITEM hRetTreeItem, hParentItem;

    if (!hTreeItem) {		//ルートを設定
        hRetTreeItem = GetRootItem();
    }
    else if (!GetChildItem(hTreeItem)) {//子供がいるか調べる
        hRetTreeItem = GetNextSiblingItem(hTreeItem);//兄弟ハンドルを取得
        if (!hRetTreeItem) {//兄弟がもういない時は親の兄弟
            hParentItem = hTreeItem;
            do {
                hParentItem = GetParentItem(hParentItem);
                if (!hParentItem) {
                    hRetTreeItem = nullptr;
                    break;
                }
                hRetTreeItem = GetNextSiblingItem(hParentItem);//兄弟ハンドルを取得
            } while (hParentItem != GetRootItem() && !hRetTreeItem);
        }
    }
    else {
        hRetTreeItem = GetChildItem(hTreeItem);//子供のハンドルを取得
    }
    if (!hRetTreeItem) {
        hRetTreeItem = GetRootItem();//再ループで必要なので戻る
    }
    return hRetTreeItem;
}

HTREEITEM CCharu3Tree::GetTruePrevItem(HTREEITEM hTreeItem)
{
    HTREEITEM hRetTreeItem = nullptr;
    if (hTreeItem) {
        hRetTreeItem = GetPrevSiblingItem(hTreeItem);
        if (!hRetTreeItem) {
            HTREEITEM hParentItem = hTreeItem;
            do {
                hParentItem = GetParentItem(hParentItem);
                if (!hParentItem) {
                    break;
                }
                hRetTreeItem = GetPrevSiblingItem(hParentItem);
            } while (hParentItem != GetRootItem() && !hRetTreeItem);
        }
    }
    if (!hRetTreeItem) {
        hRetTreeItem = GetLastSiblingItem(GetRootItem());
    }
    if (hRetTreeItem) {
        if (GetChildItem(hRetTreeItem)) {
            hRetTreeItem = GetLastDescendantItem(hRetTreeItem);
        }
    }
    return hRetTreeItem;
}

//---------------------------------------------------
//関数名	GetLastVisibleItem()
//機能		最後の可視アイテムを取得
//---------------------------------------------------
HTREEITEM CCharu3Tree::GetLastVisibleItem()
{
    HTREEITEM hRetTreeItem;
    HTREEITEM hTreeItemTmp = nullptr;
    hRetTreeItem = GetRootItem();
    while (hRetTreeItem) {
        hTreeItemTmp = hRetTreeItem;
        hRetTreeItem = GetNextVisibleItem(hRetTreeItem);//最後の可視アイテムを取得
    }
    if (hTreeItemTmp)	hRetTreeItem = hTreeItemTmp;
    return hRetTreeItem;
}

HTREEITEM CCharu3Tree::GetLastSiblingItem(HTREEITEM hTreeItem)
{
    HTREEITEM next;

    if (!hTreeItem) {
        return nullptr;
    }
    while (next = GetNextSiblingItem(hTreeItem)) {
        hTreeItem = next;
    }
    return hTreeItem;
}

HTREEITEM CCharu3Tree::GetLastDescendantItem(HTREEITEM hTreeItem)
{
    HTREEITEM next;

    if (!hTreeItem) {
        return nullptr;
    }
    while (next = GetChildItem(hTreeItem)) {
        hTreeItem = GetLastSiblingItem(next);
    }
    return hTreeItem;
}

//---------------------------------------------------
//関数名	HTREEITEM getFirstFolder(HTREEITEM hStartItem)
//機能		最初のフォルダを探す
//---------------------------------------------------
HTREEITEM CCharu3Tree::getFirstFolder(HTREEITEM hStartItem)
{
    if (!hStartItem) return nullptr;
    HTREEITEM hItem = hStartItem;
    hItem = GetChildItem(hItem);
    do { // TODO: Is it safe to use 'do'?
        STRING_DATA* dataPtr = getDataPtr(hItem);
        if (dataPtr->m_cKind & KIND_FOLDER_ALL) 	break;
        hItem = GetNextItem(hItem, TVGN_NEXT);
    } while (hItem);
    return hItem;
}

//---------------------------------------------------
//関数名	HTREEITEM getLastChild(HTREEITEM hStartItem)
//機能		子アイテムの最後を探す
//---------------------------------------------------
HTREEITEM CCharu3Tree::getLastChild(HTREEITEM hStartItem)
{
    if (!hStartItem) return nullptr;
    HTREEITEM hItem = hStartItem, hPrevItem = nullptr;
    hItem = GetChildItem(hItem);
    do { // TODO: Is it safe to use 'do'?
        STRING_DATA* dataPtr = getDataPtr(hItem);
        if (dataPtr->m_cKind & KIND_DATA_ALL) hPrevItem = hItem;
        hItem = GetNextItem(hItem, TVGN_NEXT);
    } while (hItem);
    return hPrevItem;
}

//---------------------------------------------------
//関数名	setPlugin(CString strPath)
//機能		プラグイン情報を設定する
//---------------------------------------------------
void CCharu3Tree::setPlugin(CString strPath)
{
    m_rwPlugin.reserve(20);//とりあえず20個分確保
    m_rwPlugin.clear();

    CFileFind filefind;
    BOOL more = filefind.FindFile(strPath + _T("\\*.dll"));
    while (more) {
        more = filefind.FindNextFile();
        if (!filefind.IsDirectory()) {
            HMODULE hDLL = LoadLibrary(filefind.GetFilePath());//DLLをロード
            if (hDLL) {
                //関数ポインタを取得
                isUnicode pIsUnicode = (isUnicode)GetProcAddress(hDLL, "isUnicode");
#ifdef _UNICODE
                if (pIsUnicode && pIsUnicode())
#else
                if (pIsUnicode && !pIsUnicode())
#endif
                {
                    readDataFile pReadData = (readDataFile)GetProcAddress(hDLL, "readDataFile");
                    initDLL pInitDLL = (initDLL)GetProcAddress(hDLL, "initDLL");
                    endDLL pEndDLL = (endDLL)GetProcAddress(hDLL, "endDLL");
                    writeDataFile pWriteData = (writeDataFile)GetProcAddress(hDLL, "writeDataFile");
                    getFormatName pGetFormatName = (getFormatName)GetProcAddress(hDLL, "getFormatName");
                    getExtension pGetExtension = (getExtension)GetProcAddress(hDLL, "getExtension");

                    if (pInitDLL && pEndDLL && pReadData && pWriteData && pGetFormatName && pGetExtension) {
                        TCHAR tcFormat[256], tcExt[256];
                        pGetExtension(tcExt, sizeof tcExt);
                        pGetFormatName(tcFormat, sizeof tcFormat);
                        RW_PLUGIN plugin(filefind.GetFilePath(), tcFormat, tcExt);
                        m_rwPlugin.push_back(plugin);

                        if (theApp.m_ini.m_bDebug) {
                            LOG(_T("plugin setting \"%s\""), plugin.m_strPluginFilePath.GetString());
                        }
                    }
                }
                FreeLibrary(hDLL);
            }
        }
    }
}

//---------------------------------------------------
//関数名	setImageList(POINT posSize)
//機能		イメージリストを作る
//---------------------------------------------------
void CCharu3Tree::setImageList(POINT posSize, CString strFileName, CString strPath)
{
    //アイコンのイメージリストを作る
    if (m_ImageList == nullptr) {
        ::SetCurrentDirectory(strPath);

        CBitmap Bitmap;
        m_ImageList = new CImageList;

        if (strFileName == "" || !loadBitmapFile(strFileName, &Bitmap))
            Bitmap.LoadBitmap(IDB_ICON_BITMAP);//ビットマップをロード

        BITMAP bitMap;
        Bitmap.GetBitmap(&bitMap);

        m_ImageList->Create(bitMap.bmHeight, bitMap.bmHeight, ILC_COLOR16 | ILC_MASK, 22, 2);

        m_ImageList->Add(&Bitmap, RGB(0, 255, 0));
        Bitmap.DeleteObject();
        CTreeCtrl::SetImageList(m_ImageList, TVSIL_NORMAL);
    }
}

//---------------------------------------------------
//関数名	checkMyChild(HTREEITEM hChildItem,int nMyID)
//機能		自分の子孫フォルダか調べる
//---------------------------------------------------
bool CCharu3Tree::checkMyChild(HTREEITEM hMeItem, HTREEITEM hChildItem)
{
    bool isRet = false;

    if (!hChildItem || !hMeItem) return isRet;

    while (hChildItem) {
        hChildItem = GetParentItem(hChildItem);//親を取得
        if (hMeItem == hChildItem) {//自分の子孫でした
            isRet = true;
            break;
        }
    }

    return isRet;
}

//---------------------------------------------------
//関数名	searchMyRoots(HTREEITEM hStartItem)
//機能		一番上の親を探す
//---------------------------------------------------
HTREEITEM CCharu3Tree::searchMyRoots(HTREEITEM hStartItem)
{
    HTREEITEM hRet = nullptr, hTmp;

    hTmp = hStartItem;
    while (hTmp) {
        hRet = hTmp;
        hTmp = GetParentItem(hTmp);
        if (hTmp) Expand(hTmp, TVE_COLLAPSE);
    }
    return hRet;
}

//---------------------------------------------------
//関数名	searchParentOption(HTREEITEM hStartItem,CString strOption)
//機能		親のオプションを捜していく
//---------------------------------------------------
HTREEITEM CCharu3Tree::searchParentOption(HTREEITEM hStartItem, CString strOption)
{
    strOption.MakeLower();
    HTREEITEM hRet = hStartItem;
    while (hRet) {
        if (hRet) {
            STRING_DATA data = getData(hRet);
            data.m_strMacro.MakeLower();
            int nFound = data.m_strMacro.Find(strOption);
            if (nFound >= 0) {
                nFound = data.m_strMacro.Find(_T("="), nFound);
                if (nFound >= 0) {
                    break;
                }
            }
        }
        hRet = GetParentItem(hRet);
    }
    return hRet;
}

//---------------------------------------------------
//関数名	saveDataToFile(CString strFileName,CString strPlugin)
//機能		データを保存する
//---------------------------------------------------
bool CCharu3Tree::saveDataToFile(CString strFileName, CString strPlugin, HTREEITEM hStartItem)
{
    std::list<STRING_DATA> tmplist;

    //全データを出力
    if (!hStartItem) {
        hStartItem = GetRootItem();
        //保存データをリストに変換（データの順番はツリーで持っているため）
        tree2List(hStartItem, &tmplist);
    }
    //エクスポートの場合
    else {
        STRING_DATA data = getData(hStartItem);
        data.m_nParentID = dataTree::ROOT;
        tmplist.insert(tmplist.end(), data);
        if (ItemHasChildren(hStartItem)) tree2List(GetChildItem(hStartItem), &tmplist);
    }

    if (strPlugin != DAT_FORMAT && strPlugin != DAT_FORMAT2) {
        return SaveDataWithPlugin(strFileName, strPlugin, &tmplist);
    }

    FILE* fFile = nullptr;
    if (_tfopen_s(&fFile, strFileName, _T("wb")) != 0) {
        return false;
    }
    if (nullptr == fFile) {
        return false;
    }

#ifdef _UNICODE
    fwrite(DAT_FORMAT2, strlen(DAT_FORMAT2), 1, fFile);//データ識別子
#else
    fwrite(DAT_FORMAT, strlen(DAT_FORMAT), 1, fFile);//データ識別子
#endif
    for (std::list<STRING_DATA>::iterator it = tmplist.begin(); it != tmplist.end(); it++) {

        if (it->m_cKind & KIND_ONETIME) {//ノーマル項目以外を保存
            continue;
        }
        fwrite(&it->m_cKind, sizeof(it->m_cKind), 1, fFile);//データの種類
        fwrite(&it->m_cIcon, sizeof(it->m_cIcon), 1, fFile);//アイコン種別
        fwrite(&it->m_nMyID, sizeof(it->m_nMyID), 1, fFile);//自分のID
        fwrite(&it->m_nParentID, sizeof(it->m_nParentID), 1, fFile);//親のID
        fwrite(&it->m_timeCreate, sizeof(it->m_timeCreate), 1, fFile);//作成日時
        fwrite(&it->m_timeEdit, sizeof(it->m_timeEdit), 1, fFile);//変更日時

        //タイトル
        int nDataSize = it->m_strTitle.GetLength();
#ifdef _UNICODE
        nDataSize = nDataSize * sizeof(WCHAR);
#endif
        fwrite(&nDataSize, sizeof(nDataSize), 1, fFile);
        CString strTmp = it->m_strTitle;
        fwrite(LPCTSTR(strTmp), nDataSize, 1, fFile);

        //文章
        nDataSize = it->m_strData.GetLength();
#ifdef _UNICODE
        nDataSize = nDataSize * sizeof(WCHAR);
#endif
        fwrite(&nDataSize, sizeof(nDataSize), 1, fFile);
        fwrite(LPCTSTR(it->m_strData), nDataSize, 1, fFile);

        //拡張用文字列データ
        nDataSize = it->m_strMacro.GetLength();
#ifdef _UNICODE
        nDataSize = nDataSize * sizeof(WCHAR);
#endif
        fwrite(&nDataSize, sizeof(nDataSize), 1, fFile);
        fwrite(LPCTSTR(it->m_strMacro), nDataSize, 1, fFile);
    }

    if (theApp.m_ini.m_bDebug) {
        LOG(_T("wrriten charu3 data \"%s\""), strFileName.GetString());
    }
    fclose(fFile);

    return true;
}

//---------------------------------------------------
//関数名	saveDataWithPlugin(CString strFileName,CString strPlugin)
//機能		プラグインを使ってデータを保存する
//---------------------------------------------------
bool CCharu3Tree::SaveDataWithPlugin(CString strFileName, CString strPlugin, std::list<STRING_DATA>* tmplist)
{
    bool isRet = false;
    RW_PLUGIN plugin;

    if (!getPlugin(strPlugin, &plugin)) {
        CString strRes;
        (void)strRes.LoadString(APP_MES_NOT_FOUND_WRITEPLUGIN);
        AfxMessageBox(strRes, MB_ICONEXCLAMATION, 0);
        return isRet;
    }

    HMODULE hDLL;
    writeDataFile pWriteData;

    hDLL = LoadLibrary(plugin.m_strPluginFilePath);//DLLをロード
    if (hDLL) {
        //関数ポインタを取得
        pWriteData = (writeDataFile)GetProcAddress(hDLL, "writeDataFile");

        if (pWriteData) {
            isRet = pWriteData(strFileName, tmplist);
        }
        FreeLibrary(hDLL);
    }

    if (theApp.m_ini.m_bDebug) {
        LOG(_T("wrriten plugin data \"%s\" %s %d"), strFileName.GetString(), strPlugin.GetString(), isRet);
    }
    return isRet;
}

//---------------------------------------------------
//関数名	loadDataFileDef(CString strFileName,CString strPlugin)
//機能		デフォルトリストにデータを読み込む
//---------------------------------------------------
bool CCharu3Tree::loadDataFileDef(CString strFileName, CString strPlugin) {
    DeleteAllItems();
    bool isRet = loadDataFile(strFileName, strPlugin, &m_MyStringList);
    CWnd::LockWindowUpdate();
    copyData(dataTree::ROOT, TVI_ROOT, &m_MyStringList);//ツリーにデータをセット
    CWnd::UnlockWindowUpdate();

    return isRet;
}

//---------------------------------------------------
//関数名	loadDataFile(CString strFileName,CString strPlugin)
//機能		データを読み込む
//---------------------------------------------------
bool CCharu3Tree::loadDataFile(CString strFileName, CString strPlugin, std::list<STRING_DATA>* tmplist)
{
    bool isRet = false;

    if (strPlugin != DAT_FORMAT) {
        //プラグインを使ってファイルから読み込む
        return LoadDataWithPlugin(strFileName, strPlugin, tmplist);
    }

    STRING_DATA data;
    int nDataSize;
    FILE* fFile;
    /*
        識別子
        データ種別		1byte
        アイコン種別	1byte
        ID				4byte
        親のID			4byte
        作成日時		4byte
        タイトルサイズ	4byte
        タイトル
        本文サイズ		4byte
        本文
        マクロサイズ	4byte
        マクロ
    */
    //ファイルの拡張子を検査
    char* szReadBuff;
    char strMBCS[256];
    wchar_t* szUniReadBuff;
    int  nVersion = 0;
    CString strExtention;
    strExtention = strFileName.Right(3);
    strExtention.MakeLower();
    std::list<STRING_DATA> readList;//データリスト

    if (strExtention != DAT_EXT) return isRet;

    if (_tfopen_s(&fFile, strFileName, _T("rb")) != 0)	return isRet;//ファイルを開く
    if (!fread(strMBCS, strlen(DAT_FORMAT), 1, fFile)) {
        ;//データ識別子を取得
        fclose(fFile);
        return isRet;
    }

    strMBCS[strlen(DAT_FORMAT)] = '\0';

    if (strcmp(strMBCS, DAT_FORMAT) != 0) {
        nVersion = 1;
        if (strcmp(strMBCS, DAT_FORMAT2) != 0) {
            fclose(fFile);
            return isRet;
        }
    }

    if (feof(fFile)) {
        fclose(fFile);
        return isRet;
    }

    readList.clear();

    //仮リストに読み込み
    while (!feof(fFile)) {
        if (!fread(&data.m_cKind, sizeof(data.m_cKind), 1, fFile)) break;//データの種類を読み込み
        if (!fread(&data.m_cIcon, sizeof(data.m_cIcon), 1, fFile)) break;//アイコン種別
        if (!fread(&data.m_nMyID, sizeof(data.m_nMyID), 1, fFile)) break;//自分のIDを読み込み
        if (!data.m_nMyID) data.m_nMyID = makeNewID();
        if (!fread(&data.m_nParentID, sizeof(data.m_nParentID), 1, fFile)) break;//親のIDを読み込み
        if (!fread(&data.m_timeCreate, sizeof(data.m_timeCreate), 1, fFile)) break;//作成日時を読み込み
        if (!fread(&data.m_timeEdit, sizeof(data.m_timeEdit), 1, fFile)) break;//変更日時を読み込み

        //文字列データ読み込み MBCS
        if (nVersion == 0) {
            //タイトルを読み込み
            if (!fread(&nDataSize, sizeof(nDataSize), 1, fFile)) break;
            szReadBuff = new char[nDataSize + 1];
            fread(szReadBuff, nDataSize, 1, fFile);
            szReadBuff[nDataSize] = '\0';
            data.m_strTitle = szReadBuff;
            delete[] szReadBuff;
            //文章を読み込み
            if (!fread(&nDataSize, sizeof(nDataSize), 1, fFile)) break;
            szReadBuff = new char[nDataSize + 1];
            fread(szReadBuff, nDataSize, 1, fFile);
            szReadBuff[nDataSize] = '\0';
            data.m_strData = szReadBuff;
            delete[] szReadBuff;

            //拡張用文字列データ
            if (!fread(&nDataSize, sizeof(nDataSize), 1, fFile)) break;
            szReadBuff = new char[nDataSize + 1];
            fread(szReadBuff, nDataSize, 1, fFile);
            szReadBuff[nDataSize] = '\0';
            data.m_strMacro = szReadBuff;
            delete[] szReadBuff;
        }
        //文字列データ読み込み　Unicode
        else if (nVersion == 1) {
            //タイトルを読み込み
            if (!fread(&nDataSize, sizeof(nDataSize), 1, fFile)) break;
            szUniReadBuff = new wchar_t[nDataSize + 1];
            fread(szUniReadBuff, nDataSize, 1, fFile);
            szUniReadBuff[nDataSize / sizeof(WCHAR)] = '\0';
#ifdef _UNICODE
            data.m_strTitle = szUniReadBuff;
#else
            data.m_strTitle = Text::ConvertWcharToCString(szUniReadBuff);
#endif
            delete[] szUniReadBuff;
            //文章を読み込み
            if (!fread(&nDataSize, sizeof(nDataSize), 1, fFile)) break;
            szUniReadBuff = new wchar_t[nDataSize + 1];
            fread(szUniReadBuff, nDataSize, 1, fFile);
            szUniReadBuff[nDataSize / sizeof(WCHAR)] = '\0';
#ifdef _UNICODE
            data.m_strData = szUniReadBuff;
#else
            data.m_strData = Text::ConvertWcharToCString(szUniReadBuff);
#endif
            delete[] szUniReadBuff;

            //拡張用文字列データ
            if (!fread(&nDataSize, sizeof(nDataSize), 1, fFile)) break;
            szUniReadBuff = new wchar_t[nDataSize + 1];
            fread(szUniReadBuff, nDataSize, 1, fFile);
            szUniReadBuff[nDataSize / sizeof(WCHAR)] = '\0';
#ifdef _UNICODE
            data.m_strMacro = szUniReadBuff;
#else
            data.m_strMacro = Text::ConvertWcharToCString(szUniReadBuff);
#endif
            delete[] szUniReadBuff;
        }

        if (data.m_cIcon < KIND_DEFAULT) data.m_cIcon = KIND_DEFAULT;
        if (data.m_timeCreate < 0) data.m_timeCreate = 0;
        if (data.m_timeEdit < data.m_timeCreate) data.m_timeEdit = data.m_timeCreate;

        readList.insert(readList.end(), data);//リストに追加
    }

    //本リストに登録
    tmplist->clear();
    std::list<STRING_DATA>::iterator stit;

    for (stit = readList.begin(); stit != readList.end(); stit++) {
        data = *stit;
        if (checkRedundancyID(data.m_nMyID)) {
            int nOldID = data.m_nMyID;
            data.m_nMyID = makeNewID();
            std::list<STRING_DATA>::iterator newit;
            for (newit = readList.begin(); newit != readList.end(); newit++) {
                if (newit->m_nParentID == nOldID) newit->m_nParentID = data.m_nMyID;
            }
        }
        tmplist->insert(tmplist->end(), data);//リストに追加
    }
    fclose(fFile);

    if (theApp.m_ini.m_bDebug) {
        LOG(_T("read charu3 data \"%s\" %d"), strFileName.GetString(), tmplist->size());
    }

    isRet = true;
    return isRet;
}

//---------------------------------------------------
//関数名	getPlugin(CString strName,RW_PLUGIN* pPlugin)
//機能		プラグインを検索
//---------------------------------------------------
bool CCharu3Tree::getPlugin(CString strName, RW_PLUGIN* pPlugin)
{
    for (std::vector<RW_PLUGIN>::iterator it = m_rwPlugin.begin(); it != m_rwPlugin.end(); it++) {
        if (it->m_strFormatName == strName) {
            *pPlugin = *it;
            return true;
        }
    }
    return false;
}

//---------------------------------------------------
//関数名	loadDataFilePlugin(CString strFileName,CString strPlugin)
//機能		プラグインでデータを読み込み
//---------------------------------------------------
bool CCharu3Tree::LoadDataWithPlugin(CString strFileName, CString strPlugin, std::list<STRING_DATA>* tmplist)
{
    bool isRet = false;
    RW_PLUGIN plugin;

    if (!getPlugin(strPlugin, &plugin)) {
        CString strRes;
        (void)strRes.LoadString(APP_MES_NOT_FOUND_READPLUGIN);
        AfxMessageBox(strRes, MB_ICONEXCLAMATION, 0);
        return false;
    }

    HMODULE hDLL;
    readDataFile pReadData;
    initDLL pInitDLL;
    endDLL	pEndDLL;
    time_t	timeCreate;	//作成日時
    time(&timeCreate);

    std::list<STRING_DATA>* pData;//読み込みリスト
    hDLL = LoadLibrary(plugin.m_strPluginFilePath);//DLLをロード
    if (hDLL) {
        //関数ポインタを取得
        pReadData = (readDataFile)GetProcAddress(hDLL, "readDataFile");
        pInitDLL = (initDLL)GetProcAddress(hDLL, "initDLL");
        pEndDLL = (endDLL)GetProcAddress(hDLL, "endDLL");

        if (pInitDLL && pEndDLL && pReadData) {
            pInitDLL();
            pData = pReadData(strFileName, false);//読み込み関数を呼び出し

            if (!pData) {
                pEndDLL();
                FreeLibrary(hDLL);
                return isRet;
            }
            tmplist->clear();
            TCHAR* szBuff;
            STRING_DATA data, data2;

            std::list<STRING_DATA> readList;//データリスト
            std::list<STRING_DATA>::iterator stit;
            //仮リストに登録
            for (stit = pData->begin(); stit != pData->end(); stit++) {
                if (stit->m_strTitle != "") {
                    size_t size = stit->m_strTitle.GetLength() + 1;
                    szBuff = new TCHAR[size];
                    _tcscpy_s(szBuff, size, LPCTSTR(stit->m_strTitle));
                    data.m_strTitle = szBuff;
                    delete[] szBuff;
                }
                else	data.m_strTitle = "";

                if (stit->m_strData != "") {
                    size_t size = stit->m_strData.GetLength() + 1;
                    szBuff = new TCHAR[size];
                    _tcscpy_s(szBuff, size, LPCTSTR(stit->m_strData));
                    data.m_strData = szBuff;
                    delete[] szBuff;
                }
                else	data.m_strData = "";

                if (stit->m_strMacro != "") {
                    size_t size = stit->m_strMacro.GetLength() + 1;
                    szBuff = new TCHAR[size];
                    _tcscpy_s(szBuff, size, LPCTSTR(stit->m_strMacro));
                    data.m_strMacro = szBuff;
                    delete[] szBuff;
                }
                else	data.m_strMacro = "";

                data.m_cKind = stit->m_cKind;
                data.m_nMyID = stit->m_nMyID;
                data.m_nParentID = stit->m_nParentID;

                data.m_cIcon = stit->m_cIcon;
                if (data.m_cIcon < KIND_DEFAULT) data.m_cIcon = KIND_DEFAULT;

                data.m_timeCreate = stit->m_timeCreate;
                if (data.m_timeCreate <= 0) data.m_timeCreate = timeCreate;
                data.m_timeEdit = stit->m_timeEdit;
                if (data.m_timeEdit <= 0) data.m_timeEdit = timeCreate;

                readList.insert(readList.end(), data);
            }
            pEndDLL();//メモリを開放

            //本リストに登録
            tmplist->clear();
            normalizationID(&readList, dataTree::ROOT);//IDを正規化
            for (stit = readList.begin(); stit != readList.end(); stit++) {
                tmplist->insert(tmplist->end(), *stit);//リストに追加
            }
        }

        FreeLibrary(hDLL);
    }

    if (theApp.m_ini.m_bDebug) {
        LOG(_T("read plugin data \"%s\" %s"), strFileName.GetString(), strPlugin.GetString());
    }

    isRet = true;
    return isRet;
}

int CCharu3Tree::makeNewID()
{
    (*m_pMaxID)++;
    while (checkRedundancyID(*m_pMaxID)) {
        *m_pMaxID += 10;
    }
    return *m_pMaxID;
}

//---------------------------------------------------
//関数名	checkRedundancyID(int nID)
//機能		重複IDをチェック
//---------------------------------------------------
bool CCharu3Tree::checkRedundancyID(int nID)
{
    bool isRet = false;
    std::list<STRING_DATA>::iterator it;

    for (it = m_MyStringList.begin(); it != m_MyStringList.end(); it++) {
        if (it->m_nMyID == nID) {
            isRet = true;
            break;
        }
    }
    return isRet;
}

//---------------------------------------------------
//関数名	normalizationID(list<STRING_DATA>* pList,int nParentID)
//機能		IDを正規化(ほとんどCharu2Proのデータ専用)
//---------------------------------------------------
void CCharu3Tree::normalizationID(std::list<STRING_DATA>* pList, int nParentID)
{
    std::list<STRING_DATA>::iterator it;

    for (it = pList->begin(); it != pList->end(); it++) {
        //親のIDを比較
        if (it->m_nParentID == nParentID) {
            if (checkRedundancyID(it->m_nMyID)) {//IDの重複をチェック
                int nOldID = it->m_nMyID;
                it->m_nMyID = makeNewID();//新しいIDを振る
                std::list<STRING_DATA>::iterator newit;
                for (newit = pList->begin(); newit != pList->end(); newit++) {//自分の子の親IDを変える
                    if (newit->m_nParentID == nOldID) newit->m_nParentID = it->m_nMyID;
                }
            }
            //自分の子供達を再帰処理
            if (it->m_cKind & KIND_FOLDER_ALL)	normalizationID(pList, it->m_nMyID);
        }
    }
}

//---------------------------------------------------
//関数名	convertMacroPlugin(CString strSourceData)
//機能		マクロ文字列を置換
//---------------------------------------------------
bool CCharu3Tree::convertMacroPlugin(const STRING_DATA* sourceDataPtr, CString* strRet, CString strSelect, CString strClip, CString strSoftName)
{
    RW_PLUGIN plugin;
    TCHAR* szRet;
    bool isRet = false;

    bool isFound = getPlugin(strSoftName, &plugin);

    if (!isFound) {
        CString strRes;
        (void)strRes.LoadString(APP_MES_NOT_FOUND_MACROPLUGIN);
        AfxMessageBox(strRes, MB_ICONEXCLAMATION, 0);
        return isRet;
    }

    HMODULE hDLL;
    convertMacro pConvertMacro;

    hDLL = LoadLibrary(plugin.m_strPluginFilePath);//DLLをロード
    if (hDLL) {
        //関数ポインタを取得
        pConvertMacro = (convertMacro)GetProcAddress(hDLL, "convertMacro");

        if (pConvertMacro) {
            int nSize = sourceDataPtr->m_strData.GetLength() * 10 + 10240;
            szRet = new TCHAR[nSize];
            if (szRet) {
                isRet = pConvertMacro((TCHAR*)LPCTSTR(sourceDataPtr->m_strData), szRet, nSize,
                    (TCHAR*)LPCTSTR(strSelect), (TCHAR*)LPCTSTR(strClip));
                *strRet = CString(szRet);
                delete[] szRet;
            }
        }
        FreeLibrary(hDLL);
    }
    return isRet;
}

//---------------------------------------------------
//関数名	copyData
//引数		int nParentID 親のID
//          HTREEITEM hParentTreeItem 親のポインタ
//機能		文字列データリストをフォルダ表示する
//---------------------------------------------------
void CCharu3Tree::copyData(int nParentID, HTREEITEM hParentTreeItem, std::list<STRING_DATA>* pList)
{
    int nRet = 0;
    HTREEITEM hTreeItem;
    std::list<STRING_DATA>::iterator it;

    //ツリーデータ
    TV_INSERTSTRUCT treeCtrlItem;

    //データを処理
    for (it = pList->begin(); it != pList->end(); it++) {
        //親IDと同じのだけ処理
        if (it->m_nParentID == nParentID) {
            data2TreeStruct(treeCtrlItem, it);//データを写す
            treeCtrlItem.hParent = hParentTreeItem;//親を設定

            hTreeItem = InsertItem(&treeCtrlItem);

            if (*m_pSelectID == it->m_nMyID) SelectItem(hTreeItem);

            //フォルダなら再帰呼び出し
            if (it->m_cKind & KIND_FOLDER_ALL) {
                copyData(it->m_nMyID, hTreeItem, pList);
                if (it->m_cKind & FOLDER_OPEN)	Expand(hTreeItem, TVE_EXPAND);
                else							Expand(hTreeItem, TVE_COLLAPSE);
            }
        }
    }
}

//---------------------------------------------------
//関数名	moveChildren(HTREEITEM hFromItem,HTREEITEM hToItem)
//機能		フォルダからフォルダに子供たちを移動
//---------------------------------------------------
void CCharu3Tree::moveChildren(HTREEITEM hFromItem, HTREEITEM hToItem)
{
    if (!hFromItem || !hToItem) return;
    std::list<STRING_DATA>::iterator it;
    hFromItem = GetChildItem(hFromItem);
    do {
        //データを取得
        STRING_DATA dataF = getData(hFromItem);

        //ツリーデータを取得
        TV_ITEM TreeCtrlItemFrom;
        ZeroMemory(&TreeCtrlItemFrom, sizeof(TreeCtrlItemFrom));
        TreeCtrlItemFrom.hItem = hFromItem;
        TreeCtrlItemFrom.mask = TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT;
        GetItem(&TreeCtrlItemFrom);

        //追加データを作成
        TV_INSERTSTRUCT AddTreeCtrlItem;

        ZeroMemory(&AddTreeCtrlItem, sizeof(AddTreeCtrlItem));
        AddTreeCtrlItem.hInsertAfter = hToItem;
        AddTreeCtrlItem.item.mask = TreeCtrlItemFrom.mask;
        AddTreeCtrlItem.item.iImage = TreeCtrlItemFrom.iImage;
        AddTreeCtrlItem.item.iSelectedImage = TreeCtrlItemFrom.iSelectedImage;
        AddTreeCtrlItem.item.pszText = dataF.m_strTitle.GetBuffer(dataF.m_strTitle.GetLength());
        AddTreeCtrlItem.item.lParam = TreeCtrlItemFrom.lParam;
        AddTreeCtrlItem.hParent = hToItem;//フォルダ

        //同じデータを挿入
        HTREEITEM hInsertItem = InsertItem(&AddTreeCtrlItem);
        //元にフォルダ発見
        if (ItemHasChildren(hFromItem)) {
            //ターゲットを決めて再帰コピー
            moveChildren(hFromItem, hInsertItem);
        }
        hFromItem = GetNextItem(hFromItem, TVGN_NEXT);
    } while (hFromItem);
}

//---------------------------------------------------
//関数名	copyChildren(HTREEITEM hFromItem,HTREEITEM hToItem)
//機能		フォルダからフォルダに子供たちをコピー
//---------------------------------------------------
void CCharu3Tree::copyChildren(HTREEITEM hFromItem, HTREEITEM hToItem)
{
    if (!hFromItem || !hToItem) return;
    std::list<STRING_DATA>::iterator it;
    hFromItem = GetChildItem(hFromItem);
    HTREEITEM hAddtItem = nullptr;
    while (hFromItem) {
        //データを取得
        STRING_DATA dataF = getData(hFromItem);

        //同じデータを挿入
        if (hAddtItem)
            hAddtItem = AddData(hAddtItem, dataF);
        else
            hAddtItem = AddData(hToItem, dataF, true);
        // 子もコピー（もしあれば）
        copyChildren(hFromItem, hAddtItem);
        hFromItem = GetNextItem(hFromItem, TVGN_NEXT);
    }
}

//---------------------------------------------------
//関数名	margeTreeData(HTREEITEM hTreeItem,list<STRING_DATA> *pList)
//機能		リストをマージしてツリーに反映させる
//---------------------------------------------------
// TODO: Behavior when isRoot == true has not been verified. It seems strange.
HTREEITEM CCharu3Tree::mergeTreeData(HTREEITEM hTreeItem, std::list<STRING_DATA>* pList, bool isRoot)
{
    if (pList->size() > 0) {
        int nParentID;
        if (!isRoot) {//インポートフォルダを作る
            CString strRes;
            (void)strRes.LoadString(APP_INF_IMPORT_FOLDERNAME);
            hTreeItem = AddFolder(hTreeItem, strRes);//親フォルダを作る
            if (!hTreeItem) return nullptr;
            nParentID = getDataPtr(hTreeItem)->m_nMyID;
        }
        else {//ルートに展開する
            nParentID = dataTree::ROOT;
        }
        mergeList(&m_MyStringList, pList, nParentID);
        //親のいない子は消す
        std::list<STRING_DATA>::iterator it, itNext;
        int i;
        for (i = 0, it = m_MyStringList.begin(); it != m_MyStringList.end(); i++, it++) {
            STRING_DATA data;
            data = *it;
            if (it->m_nParentID != dataTree::ROOT && !checkRedundancyID(it->m_nParentID)) {
                //親がいないのは消しちゃう
                itNext = it;
                if (i > 0) itNext--;
                m_MyStringList.erase(it);
                if (i > 0) it = itNext;
                else it = m_MyStringList.begin();
            }
        }
        CWnd::LockWindowUpdate();
        if (!isRoot) {
            copyData(nParentID, hTreeItem, &m_MyStringList);
        }
        else {
            DeleteAllItems();
            copyData(nParentID, TVI_ROOT, &m_MyStringList);
            hTreeItem = this->GetRootItem();
        }
        CWnd::UnlockWindowUpdate();
    }
    else hTreeItem = nullptr;
    return hTreeItem;
}

//---------------------------------------------------
//関数名	mergeList(list<STRING_DATA> *pMainList,list<STRING_DATA> *pList,int nParent)
//機能		リストをマージする
//---------------------------------------------------
int CCharu3Tree::mergeList(std::list<STRING_DATA>* pMainList, std::list<STRING_DATA>* pList, int nParent)
{
    int nRet = 0, nBeginID = 0;
    std::list<STRING_DATA>::iterator it;
    it = pList->begin();
    nBeginID = dataTree::ROOT;

    for (it = pList->begin(); it != pList->end(); it++) {
        if (it->m_nParentID == nBeginID)	it->m_nParentID = nParent;
        pMainList->insert(pMainList->end(), *it);
        nRet++;
    }

    return nRet;
}

void CCharu3Tree::SetText(HTREEITEM hTreeItem, CString& strText)
{
    STRING_DATA* pData = getDataPtr(hTreeItem);
    pData->m_strData = strText;
}

void CCharu3Tree::UpdateItem(HTREEITEM hTreeItem)
{
    STRING_DATA* pData = getDataPtr(hTreeItem);
    int nIcon = getIconNumber(pData->m_cKind, pData->m_cIcon);
    SetItemImage(hTreeItem, nIcon, nIcon + 1);
    SetItemText(hTreeItem, pData->m_strTitle);
    time(&pData->m_timeEdit);

    if (theApp.m_ini.m_bDebug) {
        LOG(_T("UpdateItem \"%s\""), pData->m_strTitle.GetString());
    }
}

//---------------------------------------------------
//関数名	tree2List(HTREEITEM hStartItem,list<STRING_DATA> *list)
//機能		ツリー構造体からリストを作成
//---------------------------------------------------
void CCharu3Tree::tree2List(HTREEITEM hStartItem, std::list<STRING_DATA>* tmplist, bool isAll /* = false */)
{
    if (!hStartItem) return;
    HTREEITEM hItem = hStartItem;
    do {
        STRING_DATA data = getData(hItem);
        tmplist->insert(tmplist->end(), data);
        if (ItemHasChildren(hItem) && (getDataOption(data.m_strMacro, "clearrec") != 1 || isAll)) tree2List(GetChildItem(hItem), tmplist);
        hItem = GetNextItem(hItem, TVGN_NEXT);
    } while (hItem);
}

//---------------------------------------------------
//関数名	findData(STRING_DATA *ptr)
//機能		データリストからptrが指している要素を探す
//---------------------------------------------------
std::list<STRING_DATA>::iterator CCharu3Tree::findData(STRING_DATA* dataPtr)
{
    std::list<STRING_DATA>::iterator it;
    for (it = m_MyStringList.begin(); it != m_MyStringList.end(); it++) {
        if (&*it == dataPtr) {
            break;
        }
    }
    return it;
}

//---------------------------------------------------
//関数名	void closeFolder(HTREEITEM hStartItem)
//機能		指定データ以下を閉じる
//---------------------------------------------------
void CCharu3Tree::closeFolder(HTREEITEM hStartItem)
{
    if (!hStartItem) return;
    HTREEITEM hItem = hStartItem;
    do {
        if (ItemHasChildren(hItem)) {
            STRING_DATA* dataPtr = getDataPtr(hItem);
            Expand(hItem, TVE_COLLAPSE);
            dataPtr->m_cKind &= FOLDER_OPEN ^ 0xff;
            closeFolder(GetChildItem(hItem));//再帰で閉じる
        }
        hItem = GetNextItem(hItem, TVGN_NEXT);
    } while (hItem);
}

void CCharu3Tree::SetCheckEx(HTREEITEM hItem, BOOL check)
{
    SetCheck(hItem, check);
    if (check) {
        // Add this to listItem
        m_ltCheckItems.insert(m_ltCheckItems.end(), hItem);
    }
    else {
        // Remove this from listItem
        std::list<HTREEITEM>::iterator it;
        for (it = m_ltCheckItems.begin(); it != m_ltCheckItems.end(); it++) {
            if (*it == hItem) {
                m_ltCheckItems.erase(it);
                break;
            }
        }
    }
}

void CCharu3Tree::SetCheckExRecursive(HTREEITEM hItem, BOOL check)
{
    SetCheckEx(hItem, check);
    for (hItem = GetChildItem(hItem); hItem; hItem = GetNextItem(hItem, TVGN_NEXT)) {
        SetCheckExRecursive(hItem, check);
    }
}

//---------------------------------------------------
//関数名	ToggleItemCheck(HTREEITEM hItem)
//機能		項目のチェックを切替える
//---------------------------------------------------
void CCharu3Tree::ToggleItemCheck(HTREEITEM hItem)
{
    ModifyStyle(NULL, TVS_CHECKBOXES, NULL);
    if (hItem) {
        BOOL checked = GetCheck(hItem);
        if (ItemHasChildren(hItem)) {
            /* Even if we check a folder, the selected items in the folder
               have to be removed from the list first (otherwise duplicates
               will be added), so the process of unchecking the subtree should
               be done anyway. */
            SetCheckExRecursive(hItem, FALSE);
            if (!checked) {
                SetCheckExRecursive(hItem, TRUE);
            }
        }
        else {
            SetCheckEx(hItem, !checked);
        }
    }
}

//---------------------------------------------------
//関数名	UncheckItem(HTREEITEM hItem)
//機能		項目のチェックを外す
//---------------------------------------------------
void CCharu3Tree::UncheckItem(HTREEITEM hItem)
{
    if (hItem) {
        SetCheckExRecursive(hItem, FALSE);
    }
}

//---------------------------------------------------
//関数名	void cleanupOneTimeItems(HTREEITEM hStartItem)
//機能		一時項目を消す
//---------------------------------------------------
void CCharu3Tree::cleanupOneTimeItems(HTREEITEM hStartItem, int nKind /*KIND_LOCKだと削除でなくロック処理*/)
{
    if (!hStartItem) return;
    HTREEITEM hItem = hStartItem, hPrevItem;
    do {
        if (ItemHasChildren(hItem)) {
            cleanupOneTimeItems(GetChildItem(hItem), nKind);
        }
        STRING_DATA* dataPtr = getDataPtr(hItem);
        std::list<STRING_DATA>::iterator it = findData(dataPtr);
        hPrevItem = hItem;
        hItem = GetNextItem(hItem, TVGN_NEXT);
        if (it->m_cKind & KIND_ONETIME) {
            if (nKind == KIND_LOCK) {  //ロック処理
                STRING_DATA* pData = getDataPtr(hPrevItem);
                pData->m_cKind = KIND_LOCK;
                UpdateItem(hPrevItem);
            }
            else {  //クリア処理
                m_MyStringList.erase(it);
                DeleteItem(hPrevItem);
            }
        }
    } while (hItem);
}

void CCharu3Tree::changeIcon(HTREEITEM hTreeItem, int nID)
{
    STRING_DATA* pData = getDataPtr(hTreeItem);
    pData->m_cIcon = nID;
    UpdateItem(hTreeItem);
}

//---------------------------------------------------
//関数名	getData(HTREEITEM hTreeItem)
//機能		ツリーハンドルからデータを取得
//---------------------------------------------------
STRING_DATA CCharu3Tree::getData(HTREEITEM hTreeItem)
{
    return *getDataPtr(hTreeItem);
}
STRING_DATA* CCharu3Tree::getDataPtr(HTREEITEM hTreeItem)
{
    DWORD_PTR lParam = GetItemData(hTreeItem);
    return (STRING_DATA*)lParam;
}

//---------------------------------------------------
//関数名	searchItem()
//機能		指定のアイテムを検索し、選択する
//---------------------------------------------------
HTREEITEM CCharu3Tree::searchItem(HTREEITEM hStartItem, bool backward)
{
    std::vector<CString> words;

    // Split strKeywords into words
    {
        CString& strKeywords = theApp.m_ini.m_strSearchKeywords;
        CString tokens, resToken;
        (void)tokens.LoadString(APP_INF_2BYTESPACE);
        tokens += _T(" \t\n\v\f\r");
        int curPos = 0;
        resToken = strKeywords.Tokenize(tokens, curPos);
        while (resToken != _T("")) {
            if (theApp.m_ini.m_bSearchCaseInsensitive) {
                resToken.MakeLower();
            }
            words.push_back(resToken);
            resToken = strKeywords.Tokenize(tokens, curPos);
        }
    }

    // Find
    HTREEITEM hTreeItem = hStartItem ? (backward ? GetTruePrevItem(hStartItem) : GetTrueNextItem(hStartItem)) : GetRootItem();
    HTREEITEM hEndItem = hTreeItem;
    bool found = false;
    while (hTreeItem) {
        STRING_DATA* data = getDataPtr(hTreeItem);
        if (theApp.m_ini.m_nSearchTarget & SEARCH_TARGET_NAME) {
            found = findKeywords(data->m_strTitle, words);
            if (found) {
                break;
            }
        }
        if (theApp.m_ini.m_nSearchTarget & SEARCH_TARGET_DATA) {
            found = findKeywords(data->m_strData + data->m_strMacro, words);
            if (found) {
                break;
            }
        }
        hTreeItem = backward ? GetTruePrevItem(hTreeItem) : GetTrueNextItem(hTreeItem);
        if (hTreeItem == hEndItem) {
            break;
        }
    }

    if (found && hTreeItem) {
        SelectItem(hTreeItem);
    }
    else {
        hTreeItem = nullptr;
    }
    return hTreeItem;
}

//---------------------------------------------------
//関数名	HTREEITEM serchTitle(HTREEITEM hStartItem,CString strKey)
//機能		タイトル検索(前方一致)
//---------------------------------------------------
HTREEITEM CCharu3Tree::searchTitle(HTREEITEM hStartItem, CString strKey, bool makeLower)
{
    if (!hStartItem) {
        return nullptr;
    }

    HTREEITEM hItem = hStartItem;
    do {
        CString itemName = getDataPtr(hItem)->m_strTitle;
        if (makeLower) {
            itemName.MakeLower();
        }
        if (itemName.Find(strKey) == 0) {
            return hItem; // matched
        }
        // try next
        hItem = GetTrueNextItem(hItem);
    } while (hItem && hItem != hStartItem);

    // not found
    return nullptr;
}

//---------------------------------------------------
//関数名	allIconCheck()
//機能		全てのアイコンを検査して変更する
//---------------------------------------------------
void CCharu3Tree::allIconCheck()
{
    int nSize = m_MyStringList.size();
    HTREEITEM hTreeItem = GetRootItem();
    for (int i = 0; i < nSize; hTreeItem = GetTrueNextItem(hTreeItem), i++) {
        if (hTreeItem) {
            STRING_DATA* pData = getDataPtr(hTreeItem);
            pData->m_cIcon = decideIcon(pData->m_strData);
            UpdateItem(hTreeItem);
        }
    }
}

//---------------------------------------------------
//関数名	decideIcon(CString strData)
//機能		アイコンを決定
//---------------------------------------------------
char CCharu3Tree::decideIcon(CString strData)
{
    char cIcon = -1;

    //キーマクロ
    if (strData.Find(_T("<charu2MACRO_KEY>"), 0) > -1 || strData.Find(_T("<charuMACRO_KEY>"), 0) > -1) {
        cIcon = KIND_KEY;
    }
    //マクロが含まれている
    else if (strData.Find(_T("<charu2MACRO>"), 0) > -1 || strData.Find(_T("<charuMACRO>"), 0) > -1) {
        //プラグイン
        if (strData.Find(_T("$PLUG-IN<"), 0) > -1) {
            cIcon = KIND_PLUG;
        }
        //シェル
        else if (strData.Find(_T("$SHELL<"), 0) > -1) {
            cIcon = KIND_EXE;
        }
        //関連付け
        else if (strData.Find(_T("$RELATE<"), 0) > -1) {
            cIcon = KIND_RELATE;
        }
        //選択テキスト
        else if (strData.Find(_T("$SEL"), 0) > -1) {
            cIcon = KIND_SELECT;
        }
        //クリップボード
        else if (strData.Find(_T("$CLIP"), 0) > -1) {
            cIcon = KIND_CLIP;
        }
        //日付
        else if (strData.Find(_T("$Y"), 0) > -1 || strData.Find(_T("$y"), 0) > -1 || strData.Find(_T("$WH"), 0) > -1
            || strData.Find(_T("$MM"), 0) > -1 || strData.Find(_T("$M"), 0) > -1 || strData.Find(_T("$D"), 0) > -1
            || strData.Find(_T("$ddd"), 0) > -1 || strData.Find(_T("$dd"), 0) > -1 || strData.Find(_T("$d"), 0) > -1
            || strData.Find(_T("$H"), 0) > -1 || strData.Find(_T("$h"), 0) > -1 || strData.Find(_T("$m"), 0) > -1
            || strData.Find(_T("$s"), 0) > -1 || strData.Find(_T("$ampm"), 0) > -1 || strData.Find(_T("$AMPM"), 0) > -1) {
            cIcon = KIND_DATE;
        }
    }
    return cIcon;
}

//---------------------------------------------------
//関数名	makeTitle(CString strData)
//機能		保存文字列からタイトルを作る
//---------------------------------------------------
CString CCharu3Tree::makeTitle(CString strData, int nTitleLength)
{
    CString strBuff;
    strData = strData.Left(256);
    strData.Replace(_T("　"), _T(""));
    strData.Replace(_T("\r\n"), _T(""));
    strData.Replace(_T("\r"), _T(""));
    strData.Replace(_T("\n"), _T(""));
    strData.Replace(_T("\t"), _T(" "));
    Text::CompressionSpace((TCHAR*)(LPCTSTR)strData);
    strData.TrimLeft();
    strData.TrimRight();
    if (nTitleLength > strData.GetLength())	strBuff = strData;
    else	strBuff = strData.Left(nTitleLength);

    return strBuff;
}

//---------------------------------------------------
//関数名	getOneTimeItem(bool isFirst)
//機能		最初か最後の一時項目を取得
//---------------------------------------------------
HTREEITEM CCharu3Tree::getOneTimeItem(int nType)
{
    HTREEITEM hRet = nullptr;

    //先入れ先出しの場合 FIFO 一番下の一時項目を探す
    int nSize = m_MyStringList.size();
    HTREEITEM hTreeItem = GetRootItem();
    for (int i = 0; i < nSize && hTreeItem; i++, hTreeItem = GetNextItem(hTreeItem, TVGN_NEXT)) {
        STRING_DATA* data = getDataPtr(hTreeItem);
        if (data->m_cKind & KIND_ONETIME) {
            hRet = hTreeItem;
            if (theApp.m_ini.m_bDebug) {
                LOG(_T("getOneTimeItem \"%s\" %s %d"), data->m_strTitle.GetString(), data->m_strData.GetString(), data->m_cKind);
            }
            if (nType == 2) break;
        }
    }
    return hRet;
}

//---------------------------------------------------
//関数名	hasDataOption(CString strData, CString strKind)
//機能		データにオプションプロパティがあるかどうかを返す
//---------------------------------------------------
bool CCharu3Tree::hasDataOption(CString strData, CString strKind)
{
    return strData.MakeLower().Find(strKind) >= 0;
}

//---------------------------------------------------
//関数名	getDataOption(CString strData, CString strKind)
//機能		データのオプションプロパティを取得
//---------------------------------------------------
int CCharu3Tree::getDataOption(CString strData, CString strKind)
{
    int nRet = 0, nFound, nFoundEnd;
    CString strBuff;

    nFound = strData.MakeLower().Find(strKind);
    if (nFound >= 0) {
        nFound = strData.Find(_T("="), nFound);
        if (nFound >= 0) {
            nFoundEnd = strData.Find(_T("\n"), nFound);
            if (nFoundEnd <= 0) nFoundEnd = strData.GetLength();
            strBuff = strData.Mid(nFound + 1, nFoundEnd - (nFound + 1));
            strBuff.TrimRight();
            strBuff.TrimLeft();
            if (strBuff == _T("yes")) nRet = 1;
            else if (strBuff == _T("no")) nRet = 0;
            else if (strBuff == _T("")) nRet = -1;
            else nRet = _ttoi(strBuff.GetBuffer(strBuff.GetLength()));
        }
        else nRet = -1;

        if (theApp.m_ini.m_bDebug) {
            LOG(_T("getDataOption \"%s\" %d"), strKind.GetString(), nRet);
        }
    }
    else nRet = -1;

    return nRet;
}

//---------------------------------------------------
//関数名	getDataOptionHex(CString strData,CString strKind)
//機能		データ補助設定を解読
//---------------------------------------------------
DWORD CCharu3Tree::getDataOptionHex(CString strData, CString strKind)
{
    DWORD dwRet = -1;
    int nFound, nFoundEnd;
    CString strBuff;

    strData.MakeLower();
    nFound = strData.Find(strKind);
    if (nFound >= 0) {
        nFound = strData.Find(_T("="), nFound);
        if (nFound >= 0) {
            nFoundEnd = strData.Find(_T("\n"), nFound);
            if (nFoundEnd <= 0) nFoundEnd = strData.GetLength();
            strBuff = strData.Mid(nFound + 1, nFoundEnd - (nFound + 1));
            strBuff.TrimRight();
            strBuff.TrimLeft();
            _stscanf_s(strBuff, _T("%x"), &dwRet);
        }

        if (theApp.m_ini.m_bDebug) {
            LOG(_T("getDataOptionHex \"%s\" 0x%x"), strKind.GetString(), dwRet);
        }
    }

    return dwRet;
}

//---------------------------------------------------
//関数名	getDataOptionStr(CString strData,CString strKind)
//機能		データ補助設定を解読
//---------------------------------------------------
CString CCharu3Tree::getDataOptionStr(CString strData, CString strKind)
{
    int nFound, nFoundEnd;
    CString strBuff, strRet, strDataCopy;
    strRet = _T("");

    strDataCopy = strData;
    strData.MakeLower();
    nFound = strData.Find(strKind);
    if (nFound >= 0) {
        nFound = strData.Find(_T("="), nFound);
        if (nFound >= 0) {
            nFoundEnd = strData.Find(_T("\n"), nFound);
            if (nFoundEnd <= 0) nFoundEnd = strData.GetLength();
            strBuff = strDataCopy.Mid(nFound + 1, nFoundEnd - (nFound + 1));
            strBuff.TrimRight();
            strBuff.TrimLeft();
            strRet = strBuff;
        }

        if (theApp.m_ini.m_bDebug) {
            LOG(_T("getDataOptionHex \"%s\" %s"), strKind.GetString(), strRet.GetString());
        }
    }
    return strRet;
}

//---------------------------------------------------
//関数名	archiveHistory(HTREEITEM hTreeItem)
//機能		履歴アーカイブ処理
//---------------------------------------------------
void CCharu3Tree::archiveHistory(HTREEITEM hTreeItem, int nRirekiCount)
{
    int nChildCount = getChildCount(hTreeItem, true);//履歴フォルダ直下のアイテム数を取得
    if (nChildCount >= nRirekiCount) {//階層履歴数を超えてたら
        HTREEITEM hFirstFolder = getFirstFolder(hTreeItem);
        if (!hFirstFolder || (getChildCount(hFirstFolder) >= nRirekiCount)) {//階層履歴数を超えてるか、フォルダが無い場合は新しいフォルダを作る
            if (hFirstFolder) {//挿入フォルダ位置を決める
                hFirstFolder = GetNextItem(hFirstFolder, TVGN_PREVIOUS);
                if (!hFirstFolder) hFirstFolder = hTreeItem;
            }
            else {//最初の階層フォルダの場合 最後の子アイテムを取得
                hFirstFolder = getLastChild(hTreeItem);
            }
            STRING_DATA data;
            data.m_cKind = KIND_FOLDER;
            (void)data.m_strTitle.LoadString(APP_INF_CLASS_HISTORY);
            AddData(hFirstFolder, data);//階層履歴フォルダを作成
            hFirstFolder = getFirstFolder(hTreeItem);//改めてフォルダ位置を取得
        }
        if (hFirstFolder) {//あぶれ分を移動
            HTREEITEM hLastChild = getLastChild(hTreeItem);
            STRING_DATA* pData = getDataPtr(hLastChild);

            //元データを取得
            TV_ITEM TreeCtrlItem;
            ZeroMemory(&TreeCtrlItem, sizeof(TreeCtrlItem));
            TreeCtrlItem.hItem = hLastChild;
            TreeCtrlItem.mask = TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT;
            GetItem(&TreeCtrlItem);

            //追加データを作成
            TV_INSERTSTRUCT AddTreeCtrlItem;
            ZeroMemory(&AddTreeCtrlItem, sizeof(AddTreeCtrlItem));
            AddTreeCtrlItem.item.mask = TreeCtrlItem.mask;
            AddTreeCtrlItem.item.iImage = TreeCtrlItem.iImage;
            AddTreeCtrlItem.item.iSelectedImage = TreeCtrlItem.iSelectedImage;
            AddTreeCtrlItem.item.pszText = pData->m_strTitle.GetBuffer(pData->m_strTitle.GetLength());
            AddTreeCtrlItem.item.lParam = TreeCtrlItem.lParam;
            AddTreeCtrlItem.hInsertAfter = TVI_FIRST;
            AddTreeCtrlItem.hParent = hFirstFolder;
            pData->m_nParentID = getDataPtr(hFirstFolder)->m_nMyID;
            InsertItem(&AddTreeCtrlItem);
            DeleteItem(hLastChild);
        }
    }
}

//---------------------------------------------------
//関数名	deleteExcessChildren(HTREEITEM hTreeItem,int nCount)
//機能		余計な子の数を消す
//---------------------------------------------------
void CCharu3Tree::deleteExcessChildren(HTREEITEM hTreeItem, int* nCount)
{
    if (!hTreeItem) return;
    HTREEITEM hPrevItem;
    do {
        STRING_DATA* dataPtr = getDataPtr(hTreeItem);
        if (ItemHasChildren(hTreeItem)) {
            deleteExcessChildren(GetChildItem(hTreeItem), nCount);
        }
        hPrevItem = hTreeItem;
        hTreeItem = GetNextItem(hTreeItem, TVGN_NEXT);
        if (*nCount <= 1 && !ItemHasChildren(hPrevItem)) {//子を消していきます
            DeleteData(hPrevItem);
        }
        else if (dataPtr->m_cKind & KIND_DATA_ALL)	(*nCount)--;
    } while (hTreeItem);
    return;
}

//---------------------------------------------------
//関数名	getChildCount(HTREEITEM hTreeItem)
//機能		子の数を数える
//---------------------------------------------------
int CCharu3Tree::getChildCount(HTREEITEM hTreeItem, bool isBrotherOnly)
{
    int nChildren = 0;
    if (hTreeItem) hTreeItem = GetChildItem(hTreeItem);
    if (!hTreeItem) return nChildren;
    std::list<STRING_DATA>::iterator it;
    do {
        STRING_DATA* dataPtr = getDataPtr(hTreeItem);
        if (!isBrotherOnly && ItemHasChildren(hTreeItem)) {
            nChildren += getChildCount(hTreeItem);
        }
        if (dataPtr->m_cKind & KIND_DATA_ALL)	nChildren++;
        hTreeItem = GetNextItem(hTreeItem, TVGN_NEXT);
    } while (hTreeItem);

    return nChildren;
}

//---------------------------------------------------
//関数名	moveFolderTop(HTREEITEM hTreeItem)
//機能		データをフォルダの先頭に移動
//---------------------------------------------------
HTREEITEM CCharu3Tree::moveFolderTop(HTREEITEM hTreeItem)
{
    HTREEITEM hRet = nullptr;
    if (!hTreeItem) return hRet;

    HTREEITEM hParent = GetParentItem(hTreeItem);
    if (!hParent) hParent = GetRootItem();
    STRING_DATA* dataF= getDataPtr(hTreeItem);

    //ツリーデータを取得
    TV_ITEM TreeCtrlItemFrom;
    ZeroMemory(&TreeCtrlItemFrom, sizeof(TreeCtrlItemFrom));
    TreeCtrlItemFrom.hItem = hTreeItem;
    TreeCtrlItemFrom.mask = TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT;
    GetItem(&TreeCtrlItemFrom);

    //追加データを作成
    TV_INSERTSTRUCT AddTreeCtrlItem;

    ZeroMemory(&AddTreeCtrlItem, sizeof(AddTreeCtrlItem));
    AddTreeCtrlItem.hInsertAfter = TVI_FIRST;
    AddTreeCtrlItem.item.mask = TreeCtrlItemFrom.mask;
    AddTreeCtrlItem.item.iImage = TreeCtrlItemFrom.iImage;
    AddTreeCtrlItem.item.iSelectedImage = TreeCtrlItemFrom.iSelectedImage;
    AddTreeCtrlItem.item.pszText = dataF->m_strTitle.GetBuffer(dataF->m_strTitle.GetLength());
    AddTreeCtrlItem.item.lParam = TreeCtrlItemFrom.lParam;
    AddTreeCtrlItem.hParent = hParent;//フォルダ

    //同じデータを挿入
    hRet = InsertItem(&AddTreeCtrlItem);
    DeleteItem(hTreeItem);
    return hRet;
}

//---------------------------------------------------
// CCharu3Tree メッセージ ハンドラ
//---------------------------------------------------

//---------------------------------------------------
//関数名	Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext)
//機能		クリエイト処理
//---------------------------------------------------
BOOL CCharu3Tree::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext)
{
    BOOL isRet = CWnd::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext);
    return isRet;
}

//---------------------------------------------------
//関数名	OnMouseMove(UINT nFlags, CPoint point)
//機能		マウスカーソル移動
//---------------------------------------------------
void CCharu3Tree::OnMouseMove(UINT nFlags, CPoint point)
{
    if (IsDragging()) {
        CPoint ptTree(point);

        MapWindowPoints(this, &ptTree, 1);
        m_pDragImage->DragMove(point);
        m_pDragImage->DragLeave(this);
        UINT uHitTest = TVHT_ONITEM;
        HTREEITEM hTarget = HitTest(point, &uHitTest);
        if (hTarget) {
            if (hTarget != m_hDragTarget) {
                m_nHoverTick = GetTickCount64();
            }
            STRING_DATA* pTargetData = getDataPtr(hTarget);
            RECT rect;
            GetItemRect(hTarget, &rect, FALSE);
            LONG height = rect.bottom - rect.top;
            if (pTargetData->m_cKind & KIND_FOLDER_ALL) {
                if (!(pTargetData->m_cKind & FOLDER_OPEN) && ItemHasChildren(hTarget) && GetTickCount64() - m_nHoverTick >= 1000) {
                    Expand(hTarget, TVE_EXPAND);
                    pTargetData->m_cKind |= FOLDER_OPEN;
                }
                if (pTargetData->m_cKind & FOLDER_OPEN && ItemHasChildren(hTarget)) {
                    if (ptTree.y < rect.top + height / 3) {
                        m_dragState = DragState::INSERT_BEFORE;
                    }
                    else {
                        m_dragState = DragState::PLACE_INSIDE;
                    }
                }
                else {
                    if (ptTree.y < rect.top + height / 3) {
                        m_dragState = DragState::INSERT_BEFORE;
                    }
                    else if (ptTree.y > rect.bottom - height / 3) {
                        m_dragState = DragState::INSERT_AFTER;
                    }
                    else {
                        m_dragState = DragState::PLACE_INSIDE;
                    }
                }
            }
            else {
                if (ptTree.y < rect.top + height / 2) {
                    m_dragState = DragState::INSERT_BEFORE;
                }
                else {
                    m_dragState = DragState::INSERT_AFTER;
                }
            }
            if (DragState::PLACE_INSIDE == m_dragState) {
                SelectItem(m_hDragTarget);
                SetInsertMark(NULL);
            }
            else {
                SelectItem(NULL);
                SetInsertMark(hTarget, DragState::INSERT_AFTER == m_dragState);
            }
        }
        else {
            SelectItem(NULL);
#pragma warning(push)
#pragma warning(disable:6387)
            // SetInsertMark is declared as:
            //     BOOL SetInsertMark(_In_ HTREEITEM hItem, _In_ BOOL fAfter = TRUE);
            // The specification is to remove the insert mark if hItem is NULL.
            // But SetInsertMark(NULL) warns C6387 because hItem has an _In_ annotation. Annoying!
            SetInsertMark(NULL);
#pragma warning(pop)
        }
        m_hDragTarget = hTarget;
        this->UpdateWindow();
        m_pDragImage->DragEnter(this, point);
    }
    else {
        CPoint ptTree(point);

        MapWindowPoints(this, &ptTree, 1);
        m_pDragImage->DragMove(point);
        m_pDragImage->DragLeave(this);
        UINT uHitTest = TVHT_ONITEMLABEL;
        HTREEITEM hTarget = this->HitTest(point, &uHitTest);
        if (hTarget && m_hPrevTarget != hTarget && uHitTest & 0x06) {
            ::PostMessage(this->GetParent()->m_hWnd, WM_TIPS_CHANGE, (unsigned int)hTarget, 0);
            m_hPrevTarget = hTarget;
        }
        else if (m_hPrevTarget != hTarget) {
            ::PostMessage(this->GetParent()->m_hWnd, WM_TIPS_CHANGE, NULL, 0);
        }
    }
    CTreeCtrl::OnMouseMove(nFlags, point);
}

//---------------------------------------------------
//関数名	OnBegindrag(NMHDR* pNMHDR, LRESULT* pResult)
//機能		ドラッグ開始
//---------------------------------------------------
void CCharu3Tree::OnBegindrag(NMHDR* pNMHDR, LRESULT* pResult)
{
    NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;
    m_hDragItem = pNMTreeView->itemNew.hItem;

    CPoint ptImage, ptHotSpot;

    RECT rSelItem;
    this->GetItemRect(m_hDragItem, &rSelItem, NULL);
    m_pDragImage = this->CreateDragImage(m_hDragItem);

    m_pDragImage->DragShowNolock(TRUE);
    ptHotSpot.x = pNMTreeView->ptDrag.x + rSelItem.right;
    ptHotSpot.y = pNMTreeView->ptDrag.y + rSelItem.bottom;
    m_pDragImage->SetDragCursorImage(0, ptHotSpot);
    CPoint ptCursorPos;
    GetCursorPos(&ptCursorPos);
    m_pDragImage->BeginDrag(0, CPoint(0, 0));
    m_pDragImage->DragEnter(this, pNMTreeView->ptDrag);
    SetCapture();

    m_hDragTarget = nullptr;
    m_dragState = DragState::INSERT_AFTER;
    SelectItem(NULL);

    *pResult = 0;
}

//---------------------------------------------------
//関数名	OnLButtonUp(UINT nFlags, CPoint point)
//機能		ボタンを離した
//---------------------------------------------------
void CCharu3Tree::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (!IsDragging()) return;

    m_pDragImage->DragLeave(this);
    m_pDragImage->EndDrag();
    delete m_pDragImage;
    ReleaseCapture();
#pragma warning(push)
#pragma warning(disable:6387)
    // SetInsertMark is declared as:
    //     BOOL SetInsertMark(_In_ HTREEITEM hItem, _In_ BOOL fAfter = TRUE);
    // The specification is to remove the insert mark if hItem is NULL.
    // But SetInsertMark(NULL) warns C6387 because hItem has an _In_ annotation. Annoying!
    SetInsertMark(NULL);
#pragma warning(pop)

    //アイテムの移動処理をする
    if (m_hDragTarget && m_hDragTarget != m_hDragItem) {
        //自分の子孫には移動できない
        if (!checkMyChild(m_hDragItem, m_hDragTarget)) {
            AfxGetApp()->DoWaitCursor(1);

            //データを取得
            STRING_DATA* pData = getDataPtr(m_hDragItem);
            STRING_DATA* pDataTarget = getDataPtr(m_hDragTarget);

            // 移動するツリーアイテムの属性を取得
            TV_ITEM TreeCtrlItem;
            ZeroMemory(&TreeCtrlItem, sizeof(TreeCtrlItem));
            TreeCtrlItem.hItem = m_hDragItem;
            TreeCtrlItem.mask = TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT;
            GetItem(&TreeCtrlItem);

            //追加データを作成
            TV_INSERTSTRUCT AddTreeCtrlItem;
            ZeroMemory(&AddTreeCtrlItem, sizeof(AddTreeCtrlItem));
            AddTreeCtrlItem.item.mask = TreeCtrlItem.mask;
            AddTreeCtrlItem.item.iImage = TreeCtrlItem.iImage;
            AddTreeCtrlItem.item.iSelectedImage = TreeCtrlItem.iSelectedImage;
            AddTreeCtrlItem.item.pszText = pData->m_strTitle.GetBuffer(pData->m_strTitle.GetLength());
            AddTreeCtrlItem.item.lParam = TreeCtrlItem.lParam;

            // 移動先の親と挿入位置を決定
            if (pDataTarget->m_cKind & KIND_FOLDER_ALL && DragState::PLACE_INSIDE == m_dragState) {
                AddTreeCtrlItem.hParent = m_hDragTarget;
                AddTreeCtrlItem.hInsertAfter = TVI_FIRST;
                pData->m_nParentID = pDataTarget->m_nMyID;
            }
            else {
                AddTreeCtrlItem.hParent = GetParentItem(m_hDragTarget);
                if (DragState::INSERT_BEFORE == m_dragState) {
                    AddTreeCtrlItem.hInsertAfter = GetPrevSiblingItem(m_hDragTarget);
                    if (NULL == AddTreeCtrlItem.hInsertAfter) {
                        AddTreeCtrlItem.hInsertAfter = TVI_FIRST;
                    }
                }
                else {
                    AddTreeCtrlItem.hInsertAfter = m_hDragTarget;
                }
                pData->m_nParentID = pDataTarget->m_nParentID;
            }

            // 新ツリーアイテムを挿入
            HTREEITEM hNewItem = InsertItem(&AddTreeCtrlItem);

            // 旧ツリーアイテムの子孫を新ツリーアイテムにコピー
            copyChildren(m_hDragItem, hNewItem);
            // 旧ツリーアイテムの子孫を削除
            DeleteChildren(m_hDragItem);

            // 旧ツリーアイテムを削除
            DeleteItem(m_hDragItem);

            // 新ツリーアイテムを選択
            SelectItem(hNewItem);

            AfxGetApp()->DoWaitCursor(0);
        }
    }
    m_dragState = DragState::NOT_DRAGGING;
    CTreeCtrl::OnLButtonUp(nFlags, point);
}

//---------------------------------------------------
//関数名	OnItemexpanding(NMHDR* pNMHDR, LRESULT* pResult)
//機能		フォルダ開閉処理
//---------------------------------------------------
void CCharu3Tree::OnItemexpanding(NMHDR* pNMHDR, LRESULT* pResult)
{
    NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;

    HTREEITEM hItem = pNMTreeView->itemNew.hItem;
    if (hItem) {
        STRING_DATA* dataPtr = getDataPtr(hItem);
        //閉じる
        if (pNMTreeView->action == 1)
            dataPtr->m_cKind &= FOLDER_OPEN ^ 0xff;
        //開く
        else if (pNMTreeView->action == 2)
            dataPtr->m_cKind |= FOLDER_OPEN;
    }
    *pResult = 0;
}

void CCharu3Tree::OnSelchanged(NMHDR* pNMHDR, LRESULT* pResult)
{
    NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;

    HTREEITEM hItem = pNMTreeView->itemNew.hItem;
    if (hItem) {
        STRING_DATA* dataPtr = getDataPtr(hItem);
        setSelectID(dataPtr->m_nMyID);
    }
    *pResult = 0;
}

void CCharu3Tree::OnWindowPosChanging(WINDOWPOS FAR* lpwndpos)
{
    if (lpwndpos) {
        CTreeCtrl::OnWindowPosChanging(lpwndpos);
    }
}

//---------------------------------------------------
//関数名	setScrollBar
//機能		スクロールバー設定
//---------------------------------------------------
void CCharu3Tree::setScrollBar()
{
    SCROLLINFO si;

    bool showV = theApp.m_ini.m_bScrollbarVertical;
    if (showV) {
        ZeroMemory(&si, sizeof(si));
        si.cbSize = sizeof si;
        si.fMask = SIF_PAGE;
        showV = this->GetScrollInfo(SB_VERT, &si) && si.nPage > 1;
    }
    ShowScrollBar(SB_VERT, showV);

    bool showH = theApp.m_ini.m_bScrollbarHorizontal;
    if (showH) {
        ZeroMemory(&si, sizeof(si));
        si.cbSize = sizeof si;
        si.fMask = SIF_PAGE;
        showH = this->GetScrollInfo(SB_HORZ, &si) && si.nPage > 1;
    }
    ShowScrollBar(SB_HORZ, showH);
}

//---------------------------------------------------
//関数名	OnMouseWheel
//機能		ホイール処理
//---------------------------------------------------
BOOL CCharu3Tree::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    switch (theApp.m_ini.m_nWheelBehavior) {
    case 0: // Scroll
        SendMessage(WM_VSCROLL, zDelta < 0 ? SB_LINEDOWN : SB_LINEUP, 0);
        return TRUE;
    case 1: // Change selection
        SendMessage(WM_KEYDOWN, zDelta < 0 ? VK_DOWN : VK_UP, 0);
        return TRUE;
    case 2:  // Nothing
    default:
        // return CTreeCtrl::OnMouseWheel(nFlags, zDelta, pt);
        return TRUE;
    }
}

void CCharu3Tree::OnItemexpanded(NMHDR* pNMHDR, LRESULT* pResult)
{
    NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;
    setScrollBar();

    *pResult = 0;
}
