#include "common/os.h"

#if defined(SYS_WINDOWS)

# include <windows.h>

# if !defined(__ITaskbarList3_FWD_DEFINED__)
#  define __ITaskbarList3_FWD_DEFINED__
typedef interface ITaskbarList3 ITaskbarList3;
# endif  // __ITaskbarList3_FWD_DEFINED__

# if !defined(__ITaskbarList3_INTERFACE_DEFINED__)
#  define __ITaskbarList3_INTERFACE_DEFINED__

#ifndef GUID_SECT
#if defined (__GNUC__) && (__GNUC__ <= 2 && __GNUC_MINOR__ < 95)
#define GUID_SECT __attribute__ ((section (GUID_SECTION)))
#else
#define GUID_SECT
#endif
#endif

#ifndef GUID_EXT
#if !defined(INITGUID) || (defined(INITGUID) && defined(__cplusplus))
#define GUID_EXT EXTERN_C
#else
#define GUID_EXT
#endif
#endif

GUID_EXT const GUID MTX_CLSID_TaskbarList;
static const GUID IID_ITaskbarList3 = { 0xea1afb91, 0x9e28, 0x4b86, { 0x90, 0xE9, 0x9e, 0x9f, 0x8a, 0x5e, 0xef, 0xaf } };

enum TBPFLAG {
  TBPF_NOPROGRESS    = 0x00,
  TBPF_INDETERMINATE = 0x01,
  TBPF_NORMAL        = 0x02,
  TBPF_ERROR         = 0x04,
  TBPF_PAUSED        = 0x08
};

struct ITaskbarList3Vtbl {
  BEGIN_INTERFACE

  HRESULT ( STDMETHODCALLTYPE *QueryInterface )(ITaskbarList3 * This,
                                                /* [in] */ REFIID riid,
                                                /* [annotation][iid_is][out] */
                                                void **ppvObject);

  ULONG ( STDMETHODCALLTYPE *AddRef )(ITaskbarList3 * This);

  ULONG ( STDMETHODCALLTYPE *Release )(ITaskbarList3 * This);

  HRESULT ( STDMETHODCALLTYPE *HrInit )(ITaskbarList3 * This);

  HRESULT ( STDMETHODCALLTYPE *AddTab )(ITaskbarList3 * This,
                                        /* [in] */ HWND hwnd);

  HRESULT ( STDMETHODCALLTYPE *DeleteTab )(ITaskbarList3 * This,
                                           /* [in] */ HWND hwnd);

  HRESULT ( STDMETHODCALLTYPE *ActivateTab )(ITaskbarList3 * This,
                                             /* [in] */ HWND hwnd);

  HRESULT ( STDMETHODCALLTYPE *SetActiveAlt )(ITaskbarList3 * This,
                                              /* [in] */ HWND hwnd);

  HRESULT ( STDMETHODCALLTYPE *MarkFullscreenWindow )(ITaskbarList3 * This,
                                                      /* [in] */ HWND hwnd,
                                                      /* [in] */ BOOL fFullscreen);

  HRESULT ( STDMETHODCALLTYPE *SetProgressValue )(ITaskbarList3 * This,
                                                  /* [in] */ HWND hwnd,
                                                  /* [in] */ ULONGLONG ullCompleted,
                                                  /* [in] */ ULONGLONG ullTotal);

  HRESULT ( STDMETHODCALLTYPE *SetProgressState )(ITaskbarList3 * This,
                                                  /* [in] */ HWND hwnd,
                                                  /* [in] */ TBPFLAG tbpFlags);

  HRESULT ( STDMETHODCALLTYPE *RegisterTab )(ITaskbarList3 * This,
                                             /* [in] */ HWND hwndTab,
                                             /* [in] */ HWND hwndMDI);

  HRESULT ( STDMETHODCALLTYPE *UnregisterTab )(ITaskbarList3 * This,
                                               /* [in] */ HWND hwndTab);

  HRESULT ( STDMETHODCALLTYPE *SetTabOrder )(ITaskbarList3 * This,
                                             /* [in] */ HWND hwndTab,
                                             /* [in] */ HWND hwndInsertBefore);

  HRESULT ( STDMETHODCALLTYPE *SetTabActive )(ITaskbarList3 * This,
                                              /* [in] */ HWND hwndTab,
                                              /* [in] */ HWND hwndMDI,
                                              /* [in] */ DWORD dwReserved);

  HRESULT ( STDMETHODCALLTYPE *ThumbBarAddButtons )(ITaskbarList3 * This,
                                                    /* [in] */ HWND hwnd,
                                                    /* [in] */ UINT cButtons,
                                                    /* [size_is][in] */ /* LPTHUMBBUTTON */ void * pButton);

  HRESULT ( STDMETHODCALLTYPE *ThumbBarUpdateButtons )(ITaskbarList3 * This,
                                                       /* [in] */ HWND hwnd,
                                                       /* [in] */ UINT cButtons,
                                                       /* [size_is][in] */ /* LPTHUMBBUTTON */ void * pButton);

  HRESULT ( STDMETHODCALLTYPE *ThumbBarSetImageList )(ITaskbarList3 * This,
                                                      /* [in] */ HWND hwnd,
                                                      /* [in] */ /* HIMAGELIST */ void * himl);

  HRESULT ( STDMETHODCALLTYPE *SetOverlayIcon )(ITaskbarList3 * This,
                                                /* [in] */ HWND hwnd,
                                                /* [in] */ HICON hIcon,
                                                /* [string][unique][in] */ LPCWSTR pszDescription);

  HRESULT ( STDMETHODCALLTYPE *SetThumbnailTooltip )(ITaskbarList3 * This,
                                                     /* [in] */ HWND hwnd,
                                                     /* [string][unique][in] */ LPCWSTR pszTip);

  HRESULT ( STDMETHODCALLTYPE *SetThumbnailClip )(ITaskbarList3 * This,
                                                  /* [in] */ HWND hwnd,
                                                  /* [in] */ RECT *prcClip);

  END_INTERFACE
};

interface ITaskbarList3 {
  const struct ITaskbarList3Vtbl *lpVtbl;
};

#endif  // __ITaskbarList3_INTERFACE_DEFINED__

#endif  // SYS_WINDOWS
