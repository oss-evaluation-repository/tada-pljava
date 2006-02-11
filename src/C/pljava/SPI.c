/*
 * Copyright (c) 2004, 2005, 2006 TADA AB - Taby Sweden
 * Distributed under the terms shown in the file COPYRIGHT
 * found in the root folder of this project or at
 * http://eng.tada.se/osprojects/COPYRIGHT.html
 *
 * @author Thomas Hallgren
 */
#include "org_postgresql_pljava_internal_SPI.h"
#include "pljava/backports.h"
#include "pljava/SPI.h"
#include "pljava/Invocation.h"
#include "pljava/Exception.h"
#include "pljava/type/String.h"
#include "pljava/type/TupleTable.h"

Savepoint* infant = 0;

extern void SPI_initialize(void);
void SPI_initialize(void)
{
	JNINativeMethod methods[] = {
		{
		"_exec",
	  	"(JLjava/lang/String;I)I",
	  	Java_org_postgresql_pljava_internal_SPI__1exec
		},
		{
		"_getProcessed",
		"()I",
		Java_org_postgresql_pljava_internal_SPI__1getProcessed
		},
		{
		"_getResult",
		"()I",
		Java_org_postgresql_pljava_internal_SPI__1getResult
		},
		{
		"_getTupTable",
		"()Lorg/postgresql/pljava/internal/TupleTable;",
		Java_org_postgresql_pljava_internal_SPI__1getTupTable
		},
		{ 0, 0, 0 }};

	PgObject_registerNatives("org/postgresql/pljava/internal/SPI", methods);
}

/****************************************
 * JNI methods
 ****************************************/
/*
 * Class:     org_postgresql_pljava_internal_SPI
 * Method:    _exec
 * Signature: (JLjava/lang/String;I)I
 */
JNIEXPORT jint JNICALL
Java_org_postgresql_pljava_internal_SPI__1exec(JNIEnv* env, jclass cls, jlong threadId, jstring cmd, jint count)
{
	jint result = 0;

	BEGIN_NATIVE
	char* command = String_createNTS(cmd);
	if(command != 0)
	{
		STACK_BASE_VARS
		STACK_BASE_PUSH(threadId)
		PG_TRY();
		{
			Invocation_assertConnect();
			result = (jint)SPI_exec(command, (int)count);
			if(result < 0)
				Exception_throwSPI("exec", result);
	
			pfree(command);
		}
		PG_CATCH();
		{
			Exception_throw_ERROR("SPI_exec");
		}
		PG_END_TRY();
		STACK_BASE_POP()
	}
	END_NATIVE	
	return result;
}

/*
 * Class:     org_postgresql_pljava_internal_SPI
 * Method:    _getProcessed
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_org_postgresql_pljava_internal_SPI__1getProcessed(JNIEnv* env, jclass cls)
{
	return (jint)SPI_processed;
}

/*
 * Class:     org_postgresql_pljava_internal_SPI
 * Method:    _getResult
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_org_postgresql_pljava_internal_SPI__1getResult(JNIEnv* env, jclass cls)
{
	return (jint)SPI_result;
}

/*
 * Class:     org_postgresql_pljava_internal_SPI
 * Method:    _getTupTable
 * Signature: ()Lorg/postgresql/pljava/internal/TupleTable;
 */
JNIEXPORT jobject JNICALL
Java_org_postgresql_pljava_internal_SPI__1getTupTable(JNIEnv* env, jclass cls)
{
	jobject tupleTable = 0;
	SPITupleTable* tts = SPI_tuptable;

	if(tts != 0)
	{
		BEGIN_NATIVE
		tupleTable = TupleTable_create(tts);
		SPI_freetuptable(tts);
		SPI_tuptable = 0;
		END_NATIVE
	}
	return tupleTable;
}

static void assertXid(SubTransactionId xid)
{
	if(xid != GetCurrentSubTransactionId())
	{
		/* Oops. Rollback to top level transaction.
		 */
		ereport(ERROR, (
			errcode(ERRCODE_INVALID_TRANSACTION_TERMINATION),
			errmsg("Subtransaction mismatch at txlevel %d",
				GetCurrentTransactionNestLevel())));
	}
}

Savepoint* SPI_setSavepoint(const char* name)
{
	Savepoint* sp = (Savepoint*)palloc(sizeof(Savepoint) + strlen(name));
	Invocation_assertConnect();
	sp->nestingLevel = GetCurrentTransactionNestLevel() + 1;
	strcpy(sp->name, name);
	infant = sp;
	BeginInternalSubTransaction(sp->name);
	infant = 0;
	sp->xid = GetCurrentSubTransactionId();
	return sp;
}

void SPI_releaseSavepoint(Savepoint* sp)
{
	while(sp->nestingLevel < GetCurrentTransactionNestLevel())
		ReleaseCurrentSubTransaction();

	if(sp->nestingLevel == GetCurrentTransactionNestLevel())
	{
		assertXid(sp->xid);
		ReleaseCurrentSubTransaction();
	}
	pfree(sp);
}

void SPI_rollbackSavepoint(Savepoint* sp)
{
	while(sp->nestingLevel < GetCurrentTransactionNestLevel())
		RollbackAndReleaseCurrentSubTransaction();

	if(sp->nestingLevel == GetCurrentTransactionNestLevel())
	{
		assertXid(sp->xid);
		RollbackAndReleaseCurrentSubTransaction();
	}
	SPI_restore_connection();
	pfree(sp);
}

#if (PGSQL_MAJOR_VER == 8 && PGSQL_MINOR_VER < 1)
char* SPI_getnspname(Relation rel)
{
	return get_namespace_name(RelationGetNamespace(rel));
}

#endif


