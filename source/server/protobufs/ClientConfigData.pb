
�
Error.protoData"�
ErrorMessage+
code (2.Data.ErrorMessage.CodeRcode+
type (2.Data.ErrorMessage.TypeRtype
field (	Rfield7
severity (2.Data.ErrorMessage.SeverityRseverity
message (	Rmessage"�
Code
UNKNOWN_CODE
UNAUTHORIZED�
	NOT_FOUND�
CONFLICT�
INTERNAL_SERVER_ERROR�
NOT_IMPLEMENTED�
SERVICE_UNAVAILABLE�
HTTP_VERSION_NOT_SUPPORTED�
BAD_REQUEST�"�
Type
UNKNOWN_ERROR
AUTHENTICATION_ERROR
NO_SUCH_METHOD
NO_SUCH_RESOURCE
MISSING_VALUE
INVALID_VALUE
VALUE_TOO_SMALL
VALUE_TOO_LARGE
RESOURCE_ALREADY_EXISTS	
METHOD_CALLED_TOO_OFTEN
INVALID_PARAMETERS_FORMAT&
"LOCK_REQUEST_FAILED_DUE_TO_TIMEOUT
PROTOCOL_MISMATCH
DATABASE_ERROR
SERVICE_UNAVAILABLE_TYPE
NOT_IMPLEMENTED_TYPE
SERVER_ERROR 
SHARD_UNDER_MAINTENANCE"
PLUGIN_INACTIVE#"]
Severity
LEVEL_DEBUG 

LEVEL_INFO

LEVEL_WARN
LEVEL_ERROR
LEVEL_FATALB
com.ea.simpsons.data
�	
AuthData.protoDataError.proto"q
UserIndirectData
userId (	H RuserId�%
telemetryId (	HRtelemetryId�B	
_userIdB
_telemetryId"J
AnonymousUserData%
isAnonymous (H RisAnonymous�B
_isAnonymous"
	TokenData#

sessionKey (	H R
sessionKey�+
expirationDate (HRexpirationDate�B
_sessionKeyB
_expirationDate"�
UsersResponseMessage/
user (2.Data.UserIndirectDataH Ruser�*
token (2.Data.TokenDataHRtoken�B
_userB
_token"�
LinkData@

sourceType (2.Data.LinkData.identityTypeH R
sourceType�
sourceId (	HRsourceId�J
destinationType (2.Data.LinkData.identityTypeHRdestinationType�)
destinationId (	HRdestinationId�"-
identityType

TNT_ID 
NUCLEUS_TOKENB
_sourceTypeB
	_sourceIdB
_destinationTypeB
_destinationId"�
LinkUserResponseMessage
userId (	H RuserId�#

externalId (	HR
externalId�'
externalType (	HRexternalType�-
error (2.Data.ErrorMessageHRerror�B	
_userIdB
_externalIdB
_externalTypeB
_error"|
DeleteUserResponseMessage
userId (	H RuserId�-
error (2.Data.ErrorMessageHRerror�B	
_userIdB
_errorB!
com.eamobile.mayhem.plugin.databproto3