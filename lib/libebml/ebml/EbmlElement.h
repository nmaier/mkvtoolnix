/****************************************************************************
** libebml : parse EBML files, see http://embl.sourceforge.net/
**
** <file/class description>
**
** Copyright (C) 2002-2010 Steve Lhomme.  All rights reserved.
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License as published by the Free Software Foundation; either
** version 2.1 of the License, or (at your option) any later version.
**
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** Lesser General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public
** License along with this library; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**
** See http://www.matroska.org/license/lgpl/ for LGPL licensing information.
**
** Contact license@matroska.org if any conditions of this licensing are
** not clear to you.
**
**********************************************************************/

/*!
	\file
	\version \$Id: EbmlElement.h 722 2011-03-27 16:43:10Z robux4 $
	\author Steve Lhomme     <robux4 @ users.sf.net>
*/
#ifndef LIBEBML_ELEMENT_H
#define LIBEBML_ELEMENT_H

#include "EbmlTypes.h"
#include "EbmlId.h"
#include "IOCallback.h"

START_LIBEBML_NAMESPACE

/*!
	\brief The size of the EBML-coded length
*/
int EBML_DLL_API CodedSizeLength(uint64 Length, unsigned int SizeLength, bool bSizeIsFinite = true);

/*!
	\brief The coded value of the EBML-coded length
	\note The size of OutBuffer must be 8 octets at least
*/
int EBML_DLL_API CodedValueLength(uint64 Length, int CodedSize, binary * OutBuffer);

/*!
	\brief Read an EBML-coded value from a buffer
	\return the value read
*/
uint64 EBML_DLL_API ReadCodedSizeValue(const binary * InBuffer, uint32 & BufferSize, uint64 & SizeUnknown);

/*!
	\brief The size of the EBML-coded signed length
*/
int EBML_DLL_API CodedSizeLengthSigned(int64 Length, unsigned int SizeLength);

/*!
	\brief The coded value of the EBML-coded signed length
	\note the size of OutBuffer must be 8 octets at least
*/
int EBML_DLL_API CodedValueLengthSigned(int64 Length, int CodedSize, binary * OutBuffer);

/*!
	\brief Read a signed EBML-coded value from a buffer
	\return the value read
*/
int64 EBML_DLL_API ReadCodedSizeSignedValue(const binary * InBuffer, uint32 & BufferSize, uint64 & SizeUnknown);

class EbmlStream;
class EbmlSemanticContext;
class EbmlElement;

extern const EbmlSemanticContext Context_EbmlGlobal;

#define DEFINE_xxx_CONTEXT(x,global) \
    const EbmlSemanticContext Context_##x = EbmlSemanticContext(countof(ContextList_##x), ContextList_##x, NULL, global, NULL); \

#define DEFINE_xxx_MASTER(x,id,idl,parent,name,global) \
    const EbmlId Id_##x    (id, idl); \
    const EbmlSemanticContext Context_##x = EbmlSemanticContext(countof(ContextList_##x), ContextList_##x, &Context_##parent, global, &EBML_INFO(x)); \
    const EbmlCallbacks x::ClassInfos(x::Create, Id_##x, name, Context_##x); \
    x::x() :EbmlMaster(Context_##x) {}

#define DEFINE_xxx_MASTER_CONS(x,id,idl,parent,name,global) \
    const EbmlId Id_##x    (id, idl); \
    const EbmlSemanticContext Context_##x = EbmlSemanticContext(countof(ContextList_##x), ContextList_##x, &Context_##parent, global, &EBML_INFO(x)); \
    const EbmlCallbacks x::ClassInfos(x::Create, Id_##x, name, Context_##x);

#define DEFINE_xxx_MASTER_ORPHAN(x,id,idl,name,global) \
    const EbmlId Id_##x    (id, idl); \
    const EbmlSemanticContext Context_##x = EbmlSemanticContext(countof(ContextList_##x), ContextList_##x, NULL, global, &EBML_INFO(x)); \
    const EbmlCallbacks x::ClassInfos(x::Create, Id_##x, name, Context_##x); \

#define DEFINE_xxx_CLASS(x,id,idl,parent,name,global) \
    const EbmlId Id_##x    (id, idl); \
    const EbmlSemanticContext Context_##x = EbmlSemanticContext(0, NULL, &Context_##parent, global, &EBML_INFO(x)); \
    const EbmlCallbacks x::ClassInfos(x::Create, Id_##x, name, Context_##x); \
    x::x() {}

#define DEFINE_xxx_CLASS_CONS(x,id,idl,parent,name,global) \
    const EbmlId Id_##x    (id, idl); \
    const EbmlSemanticContext Context_##x = EbmlSemanticContext(0, NULL, &Context_##parent, global, &EBML_INFO(x)); \
    const EbmlCallbacks x::ClassInfos(x::Create, Id_##x, name, Context_##x);

#define DEFINE_xxx_UINTEGER_DEF(x,id,idl,parent,name,global,defval) \
    const EbmlId Id_##x    (id, idl); \
    const EbmlSemanticContext Context_##x = EbmlSemanticContext(0, NULL, &Context_##parent, global, &EBML_INFO(x)); \
    const EbmlCallbacks x::ClassInfos(x::Create, Id_##x, name, Context_##x); \
    x::x() :EbmlUInteger(defval) {}

#define DEFINE_xxx_SINTEGER_DEF(x,id,idl,parent,name,global,defval) \
    const EbmlId Id_##x    (id, idl); \
    const EbmlSemanticContext Context_##x = EbmlSemanticContext(0, NULL, &Context_##parent, global, &EBML_INFO(x)); \
    const EbmlCallbacks x::ClassInfos(x::Create, Id_##x, name, Context_##x); \
    x::x() :EbmlSInteger(defval) {}

#define DEFINE_xxx_STRING_DEF(x,id,idl,parent,name,global,defval) \
    const EbmlId Id_##x    (id, idl); \
    const EbmlSemanticContext Context_##x = EbmlSemanticContext(0, NULL, &Context_##parent, global, &EBML_INFO(x)); \
    const EbmlCallbacks x::ClassInfos(x::Create, Id_##x, name, Context_##x); \
    x::x() :EbmlString(defval) {}

#define DEFINE_xxx_FLOAT_DEF(x,id,idl,parent,name,global,defval) \
    const EbmlId Id_##x    (id, idl); \
    const EbmlSemanticContext Context_##x = EbmlSemanticContext(0, NULL, &Context_##parent, global, &EBML_INFO(x)); \
    const EbmlCallbacks x::ClassInfos(x::Create, Id_##x, name, Context_##x); \
    x::x() :EbmlFloat(defval) {}

#define DEFINE_xxx_CLASS_GLOBAL(x,id,idl,name,global) \
    const EbmlId Id_##x    (id, idl); \
    const EbmlCallbacks x::ClassInfos(x::Create, Id_##x, name, Context_EbmlGlobal); \

#define DEFINE_xxx_CLASS_ORPHAN(x,id,idl,name,global) \
    const EbmlId Id_##x    (id, idl); \
    const EbmlSemanticContext Context_##x = EbmlSemanticContext(0, NULL, NULL, global, NULL); \
    const EbmlCallbacks x::ClassInfos(x::Create, Id_##x, name, Context_##x); \

#define DEFINE_EBML_CONTEXT(x)                             DEFINE_xxx_CONTEXT(x,*GetEbmlGlobal_Context)
#define DEFINE_EBML_MASTER(x,id,idl,parent,name)           DEFINE_xxx_MASTER(x,id,idl,parent,name,*GetEbmlGlobal_Context)
#define DEFINE_EBML_MASTER_ORPHAN(x,id,idl,name)           DEFINE_xxx_MASTER_ORPHAN(x,id,idl,name,*GetEbmlGlobal_Context)
#define DEFINE_EBML_CLASS(x,id,idl,parent,name)            DEFINE_xxx_CLASS(x,id,idl,parent,name,*GetEbmlGlobal_Context)
#define DEFINE_EBML_CLASS_GLOBAL(x,id,idl,name)            DEFINE_xxx_CLASS_GLOBAL(x,id,idl,name,*GetEbmlGlobal_Context)
#define DEFINE_EBML_CLASS_ORPHAN(x,id,idl,name)            DEFINE_xxx_CLASS_ORPHAN(x,id,idl,name,*GetEbmlGlobal_Context)
#define DEFINE_EBML_UINTEGER_DEF(x,id,idl,parent,name,val) DEFINE_xxx_UINTEGER_DEF(x,id,idl,parent,name,*GetEbmlGlobal_Context,val)
#define DEFINE_EBML_STRING_DEF(x,id,idl,parent,name,val)   DEFINE_xxx_STRING_DEF(x,id,idl,parent,name,*GetEbmlGlobal_Context,val)
#define DEFINE_EBML_BINARY_CONS(x,id,idl,parent,name)      DEFINE_xxx_CLASS_CONS(x,id,idl,parent,name,*GetEbmlGlobal_Context)

#define DEFINE_SEMANTIC_CONTEXT(x)
#define DEFINE_START_SEMANTIC(x)     static const EbmlSemantic ContextList_##x[] = {
#define DEFINE_END_SEMANTIC(x)       };
#define DEFINE_SEMANTIC_ITEM(m,u,c)  EbmlSemantic(m, u, EBML_INFO(c)),

#define DECLARE_EBML_MASTER(x)  class EBML_DLL_API x : public EbmlMaster { \
  public: \
    x();
#define DECLARE_EBML_UINTEGER(x)  class EBML_DLL_API x : public EbmlUInteger { \
  public: \
    x();
#define DECLARE_EBML_STRING(x)    class EBML_DLL_API x : public EbmlString { \
  public: \
    x();
#define DECLARE_EBML_BINARY(x)    class EBML_DLL_API x : public EbmlBinary { \
  public: \
    x();

#if defined(EBML_STRICT_API)
#define EBML_CONCRETE_CLASS(Type) \
    public: \
        virtual const EbmlSemanticContext &Context() const {return ClassInfos.GetContext();} \
        virtual const char *DebugName() const {return ClassInfos.GetName();} \
		virtual operator const EbmlId &() const {return ClassInfos.ClassId();} \
        virtual EbmlElement & CreateElement() const {return Create();} \
        virtual EbmlElement * Clone() const { return new Type(*this); } \
		static EbmlElement & Create() {return *(new Type);} \
        static const EbmlCallbacks & ClassInfo() {return ClassInfos;} \
        static const EbmlId & ClassId() {return ClassInfos.ClassId();} \
    private: \
		static const EbmlCallbacks ClassInfos; \

#define EBML_CONCRETE_DUMMY_CLASS(Type) \
    public: \
        virtual const EbmlSemanticContext &Context() const {return *static_cast<EbmlSemanticContext*>(NULL);} \
        virtual const char *DebugName() const {return "DummyElement";} \
		virtual operator const EbmlId &(); \
        virtual EbmlElement & CreateElement() const {return Create();} \
        virtual EbmlElement * Clone() const { return new Type(*this); } \
		static EbmlElement & Create() {return *(new Type);} \
        static const EbmlId & ClassId(); \
		static const EbmlCallbacks ClassInfos; \


#define EBML_INFO(ref)             ref::ClassInfo()
#define EBML_ID(ref)               ref::ClassId()
#define EBML_CLASS_SEMCONTEXT(ref) Context_##ref
#define EBML_CLASS_CONTEXT(ref)    ref::ClassInfo().GetContext()
#define EBML_CLASS_CALLBACK(ref)   ref::ClassInfo()
#define EBML_CONTEXT(e) (e)->Context()
#define EBML_NAME(e)    (e)->DebugName()

#define EBML_INFO_ID(cb)      (cb).ClassId()
#define EBML_INFO_NAME(cb)    (cb).GetName()
#define EBML_INFO_CREATE(cb)  (cb).NewElement()
#define EBML_INFO_CONTEXT(cb) (cb).GetContext()

#define EBML_SEM_UNIQUE(s)  (s).IsUnique()
#define EBML_SEM_CONTEXT(s) ((const EbmlCallbacks &)(s)).GetContext()
#define EBML_SEM_CREATE(s)  (s).Create()

#define EBML_CTX_SIZE(c)       (c).GetSize()
#define EBML_CTX_MASTER(c)     (c).GetMaster()
#define EBML_CTX_PARENT(c)     (c).Parent()
#define EBML_CTX_IDX(c,i)      (c).GetSemantic(i)
#define EBML_CTX_IDX_INFO(c,i) (const EbmlCallbacks &)((c).GetSemantic(i))
#define EBML_CTX_IDX_ID(c,i)   ((const EbmlCallbacks &)((c).GetSemantic(i))).ClassId()
#else
#define EBML_CONCRETE_CLASS(Type) \
    public: \
		virtual const EbmlCallbacks & Generic() const {return ClassInfos;} \
		virtual operator const EbmlId &() const {return ClassInfos.GlobalId;} \
        virtual EbmlElement & CreateElement() const {return Create();} \
        virtual EbmlElement * Clone() const { return new Type(*this); } \
		static EbmlElement & Create() {return *(new Type);} \
		static const EbmlCallbacks ClassInfos; \

#define EBML_CONCRETE_DUMMY_CLASS(Type) \
    public: \
		virtual const EbmlCallbacks & Generic() const {return ClassInfos;} \
		virtual operator const EbmlId &(); \
        virtual EbmlElement & CreateElement() const {return Create();} \
        virtual EbmlElement * Clone() const { return new Type(*this); } \
		static EbmlElement & Create() {return *(new Type);} \
		static const EbmlCallbacks ClassInfos; \


#define EBML_INFO(ref)             ref::ClassInfos
#define EBML_ID(ref)               ref::ClassInfos.GlobalId
#define EBML_CLASS_SEMCONTEXT(ref) Context_##ref
#define EBML_CLASS_CONTEXT(ref)    ref::ClassInfos.Context
#define EBML_CLASS_CALLBACK(ref)   ref::ClassInfos
#define EBML_CONTEXT(e)            (e)->Generic().Context
#define EBML_NAME(e)               (e)->Generic().DebugName

#define EBML_INFO_ID(cb)      (cb).GlobalId
#define EBML_INFO_NAME(cb)    (cb).DebugName
#define EBML_INFO_CREATE(cb)  (cb).Create()
#define EBML_INFO_CONTEXT(cb) (cb).Context

#define EBML_SEM_UNIQUE(s)  (s).Unique
#define EBML_SEM_CONTEXT(s) (s).GetCallbacks.Context
#define EBML_SEM_CREATE(s)  (s).Create()

#define EBML_CTX_SIZE(c)       (c).Size
#define EBML_CTX_MASTER(c)     (c).MasterElt
#define EBML_CTX_PARENT(c)     (c).UpTable
#define EBML_CTX_IDX(c,i)      (c).MyTable[(i)]
#define EBML_CTX_IDX_INFO(c,i) (c).MyTable[(i)].GetCallbacks
#define EBML_CTX_IDX_ID(c,i)   (c).MyTable[(i)].GetCallbacks.GlobalId
#endif

#if !defined(INVALID_FILEPOS_T)
#define INVALID_FILEPOS_T 0
#endif

#define EBML_DEF_CONS
#define EBML_DEF_SEP
#define EBML_DEF_PARAM
#define EBML_DEF_BINARY_INIT
#define EBML_DEF_BINARY_CTX(x)
#define EBML_DEF_SINTEGER(x)
#define EBML_DEF_BINARY(x)
#define EBML_EXTRA_PARAM
#define EBML_EXTRA_CALL
#define EBML_EXTRA_DEF

// functions for generic handling of data (should be static to all classes)
/*!
	\todo Handle default value
*/
class EBML_DLL_API EbmlCallbacks {
	public:
		EbmlCallbacks(EbmlElement & (*Creator)(), const EbmlId & aGlobalId, const char * aDebugName, const EbmlSemanticContext & aContext);

        inline const EbmlId & ClassId() const { return GlobalId; }
        inline const EbmlSemanticContext & GetContext() const { return Context; }
        inline const char * GetName() const { return DebugName; }
        inline EbmlElement & NewElement() const { return Create(); }

#if defined(EBML_STRICT_API)
    private:
#endif
		EbmlElement & (*Create)();
		const EbmlId & GlobalId;
		const char * DebugName;
		const EbmlSemanticContext & Context;
};

/*!
	\brief contains the semantic informations for a given level and all sublevels
	\todo move the ID in the element class
*/
class EBML_DLL_API EbmlSemantic {
	public:
		EbmlSemantic(bool aMandatory, bool aUnique, const EbmlCallbacks & aGetCallbacks)
			:Mandatory(aMandatory), Unique(aUnique), GetCallbacks(aGetCallbacks) {}

        inline bool IsMandatory() const { return Mandatory; }
        inline bool IsUnique() const { return Unique; }
        inline EbmlElement & Create() const { return EBML_INFO_CREATE(GetCallbacks); }
        inline operator const EbmlCallbacks &() const { return GetCallbacks; }

#if defined(EBML_STRICT_API)
    private:
#endif
		bool Mandatory; ///< wether the element is mandatory in the context or not
		bool Unique;
		const EbmlCallbacks & GetCallbacks;
};

typedef const class EbmlSemanticContext & (*_GetSemanticContext)();

/*!
	Context of the element
	\todo allow more than one parent ?
*/
class EBML_DLL_API EbmlSemanticContext {
	public:
		EbmlSemanticContext(size_t aSize,
			const EbmlSemantic *aMyTable,
			const EbmlSemanticContext *aUpTable,
			const _GetSemanticContext aGetGlobalContext,
			const EbmlCallbacks *aMasterElt)
			: GetGlobalContext(aGetGlobalContext), MyTable(aMyTable), Size(aSize),
			  UpTable(aUpTable), MasterElt(aMasterElt) {}

		bool operator!=(const EbmlSemanticContext & aElt) const {
			return ((Size != aElt.Size) || (MyTable != aElt.MyTable) ||
				(UpTable != aElt.UpTable) || (GetGlobalContext != aElt.GetGlobalContext) |
				(MasterElt != aElt.MasterElt));
		}

        inline size_t GetSize() const { return Size; }
        inline const EbmlCallbacks* GetMaster() const { return MasterElt; }
        inline const EbmlSemanticContext* Parent() const { return UpTable; }
        const EbmlSemantic & GetSemantic(size_t i) const;

		const _GetSemanticContext GetGlobalContext; ///< global elements supported at this level

#if defined(EBML_STRICT_API)
    private:
#endif
        const EbmlSemantic *MyTable; ///< First element in the table
		size_t Size;          ///< number of elements in the table
		const EbmlSemanticContext *UpTable; ///< Parent element
		/// \todo replace with the global context directly
		const EbmlCallbacks *MasterElt;
};

/*!
	\class EbmlElement
	\brief Hold basic informations about an EBML element (ID + length)
*/
class EBML_DLL_API EbmlElement {
	public:
		EbmlElement(uint64 aDefaultSize, bool bValueSet = false);
		virtual ~EbmlElement();

		/// Set the minimum length that will be used to write the element size (-1 = optimal)
		void SetSizeLength(int NewSizeLength) {SizeLength = NewSizeLength;}
		int GetSizeLength() const {return SizeLength;}

		static EbmlElement * FindNextElement(IOCallback & DataStream, const EbmlSemanticContext & Context, int & UpperLevel, uint64 MaxDataSize, bool AllowDummyElt, unsigned int MaxLowerLevel = 1);
		static EbmlElement * FindNextID(IOCallback & DataStream, const EbmlCallbacks & ClassInfos, uint64 MaxDataSize);

		/*!
			\brief find the next element with the same ID
		*/
		EbmlElement * FindNext(IOCallback & DataStream, uint64 MaxDataSize);

		EbmlElement * SkipData(EbmlStream & DataStream, const EbmlSemanticContext & Context, EbmlElement * TestReadElt = NULL, bool AllowDummyElt = false);

		/*!
			\brief Give a copy of the element, all data inside the element is copied
			\return NULL if there is not enough memory
		*/
		virtual EbmlElement * Clone() const = 0;

		virtual operator const EbmlId &() const = 0;
#if defined(EBML_STRICT_API)
        virtual const char *DebugName() const = 0;
        virtual const EbmlSemanticContext &Context() const = 0;
#else
		/// return the generic callback to monitor a derived class
		virtual const EbmlCallbacks & Generic() const = 0;
#endif
        virtual EbmlElement & CreateElement() const = 0;

		// by default only allow to set element as finite (override when needed)
		virtual bool SetSizeInfinite(bool bIsInfinite = true) {return !bIsInfinite;}

		virtual bool ValidateSize() const = 0;

		uint64 GetElementPosition() const {
			return ElementPosition;
		}

		uint64 ElementSize(bool bWithDefault = false) const; /// return the size of the header+data, before writing

		filepos_t Render(IOCallback & output, bool bWithDefault = false, bool bKeepPosition = false, bool bForceRender = false);

		virtual filepos_t UpdateSize(bool bWithDefault = false, bool bForceRender = false) = 0; /// update the Size of the Data stored
		virtual filepos_t GetSize() const {return Size;} /// return the size of the data stored in the element, on reading

		virtual filepos_t ReadData(IOCallback & input, ScopeMode ReadFully = SCOPE_ALL_DATA) = 0;
		virtual void Read(EbmlStream & inDataStream, const EbmlSemanticContext & Context, int & UpperEltFound, EbmlElement * & FoundElt, bool AllowDummyElt = false, ScopeMode ReadFully = SCOPE_ALL_DATA);

		bool IsLocked() const {return bLocked;}
		void Lock(bool bLock = true) { bLocked = bLock;}

		/*!
			\brief default comparison for elements that can't be compared
		*/
		virtual bool IsSmallerThan(const EbmlElement *Cmp) const;
		static bool CompareElements(const EbmlElement *A, const EbmlElement *B);

		virtual bool IsDummy() const {return false;}
		virtual bool IsMaster() const {return false;}

		uint8 HeadSize() const {
			return EBML_ID_LENGTH((const EbmlId&)*this) + CodedSizeLength(Size, SizeLength, bSizeIsFinite);
		} /// return the size of the head, on reading/writing

		/*!
			\brief Force the size of an element
			\warning only possible if the size is "undefined"
		*/
		bool ForceSize(uint64 NewSize);

		filepos_t OverwriteHead(IOCallback & output, bool bKeepPosition = false);

		/*!
			\brief void the content of the element (replace by EbmlVoid)
		*/
		uint64 VoidMe(IOCallback & output, bool bWithDefault = false);

		bool DefaultISset() const {return DefaultIsSet;}
		virtual bool IsDefaultValue() const = 0;
		bool IsFiniteSize() const {return bSizeIsFinite;}

		/*!
			\brief set the default size of an element
		*/
		virtual void SetDefaultSize(uint64 aDefaultSize) {DefaultSize = aDefaultSize;}

		bool ValueIsSet() const {return bValueIsSet;}

		inline uint64 GetEndPosition() const {
			assert(bSizeIsFinite); // we don't know where the end is
			return SizePosition + CodedSizeLength(Size, SizeLength, bSizeIsFinite) + Size;
		}

	protected:
		/*!
			\brief find any element in the stream
			\return a DummyRawElement if the element is unknown or NULL if the element dummy is not allowed
		*/
		static EbmlElement *CreateElementUsingContext(const EbmlId & aID, const EbmlSemanticContext & Context, int & LowLevel, bool IsGlobalContext, bool bAllowDummy = false, unsigned int MaxLowerLevel = 1);

		filepos_t RenderHead(IOCallback & output, bool bForceRender, bool bWithDefault = false, bool bKeepPosition = false);
		filepos_t MakeRenderHead(IOCallback & output, bool bKeepPosition);

		/*!
			\brief prepare the data before writing them (in case it's not already done by default)
		*/
		virtual filepos_t RenderData(IOCallback & output, bool bForceRender, bool bWithDefault = false) = 0;

		/*!
			\brief special constructor for cloning
		*/
		EbmlElement(const EbmlElement & ElementToClone);

        inline uint64 GetDefaultSize() const {return DefaultSize;}
        inline void SetSize_(uint64 aSize) {Size = aSize;}
        inline void SetValueIsSet(bool Set = true) {bValueIsSet = Set;}
        inline void SetDefaultIsSet(bool Set = true) {DefaultIsSet = Set;}
        inline void SetSizeIsFinite(bool Set = true) {bSizeIsFinite = Set;}
        inline uint64 GetSizePosition() const {return SizePosition;}

#if defined(EBML_STRICT_API)
	private:
#endif
		uint64 Size;        ///< the size of the data to write
		uint64 DefaultSize; ///< Minimum data size to fill on rendering (0 = optimal)
		int SizeLength; /// the minimum size on which the size will be written (0 = optimal)
		bool bSizeIsFinite;
		uint64 ElementPosition;
		uint64 SizePosition;
		bool bValueIsSet;
		bool DefaultIsSet;
		bool bLocked;
};

END_LIBEBML_NAMESPACE

#endif // LIBEBML_ELEMENT_H
