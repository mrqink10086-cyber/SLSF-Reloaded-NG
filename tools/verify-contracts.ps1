[CmdletBinding()]
param(
    [switch]$NegativeFixtures,
    [string]$EspPath = ''
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$contractsRoot = Join-Path $repoRoot 'contracts'
$fixturesRoot = Join-Path $repoRoot 'tests/fixtures/contracts'
$script:checkedNegative = 0

function Fail([string]$Message) {
    throw "CONTRACT-REJECT: $Message"
}

function Check([bool]$Condition, [string]$Message) {
    if (-not $Condition) { Fail $Message }
}

function Check-Equal($Actual, $Expected, [string]$Message) {
    if ([string]$Actual -cne [string]$Expected) {
        Fail ("{0} (expected '{1}', got '{2}')" -f $Message, $Expected, $Actual)
    }
}

function Check-Sequence($Actual, $Expected, [string]$Message) {
    $a = @($Actual)
    $e = @($Expected)
    if ($a.Count -ne $e.Count) {
        Fail ("{0} count (expected {1}, got {2})" -f $Message, $e.Count, $a.Count)
    }
    for ($i = 0; $i -lt $a.Count; $i++) {
        if ([string]$a[$i] -cne [string]$e[$i]) {
            Fail ("{0}[{1}] (expected '{2}', got '{3}')" -f $Message, $i, $e[$i], $a[$i])
        }
    }
}

function Read-Json([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        Fail "missing file: $Path"
    }
    try {
        return (Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json)
    }
    catch {
        Fail "invalid JSON in ${Path}: $($_.Exception.Message)"
    }
}

function Load-Documents {
    return [pscustomobject]@{
        Api     = Read-Json (Join-Path $contractsRoot 'legacy-api.json')
        Events  = Read-Json (Join-Path $contractsRoot 'legacy-events.json')
        Records = Read-Json (Join-Path $contractsRoot 'legacy-records.json')
        Rules   = Read-Json (Join-Path $contractsRoot 'legacy-rules.json')
        Cases   = Read-Json (Join-Path $fixturesRoot 'legacy-cases.json')
        Signatures = Read-Json (Join-Path $fixturesRoot 'api-signatures.json')
    }
}

function Check-Source($Api, $Records) {
    $expectedHash = '8ebd5ab79740e85a2a8d553e47391bdfdecd82d278e30bcfcd239c28fb544812'
    $expectedSize = 23775
    Check-Equal $Api.source.espSha256 $expectedHash 'api source ESP SHA-256'
    Check-Equal $Api.source.espSize $expectedSize 'api source ESP size'
    Check-Equal $Records.source.espSha256 $expectedHash 'records source ESP SHA-256'
    Check-Equal $Records.source.espSize $expectedSize 'records source ESP size'
    if (-not [string]::IsNullOrWhiteSpace($EspPath)) {
        Check (Test-Path -LiteralPath $EspPath) "source ESP missing: $EspPath"
        $item = Get-Item -LiteralPath $EspPath
        Check-Equal $item.Length $expectedSize 'source ESP size on disk'
        Check-Equal (Get-FileHash -LiteralPath $EspPath -Algorithm SHA256).Hash.ToLowerInvariant() $expectedHash 'source ESP SHA-256 on disk'
    }
}

function Get-ParamSignature($Parameters) {
    $parts = @()
    foreach ($p in @($Parameters)) {
        $default = ''
        if ($null -ne $p.default) { $default = [string]$p.default }
        $parts += ('{0}|{1}|{2}' -f $p.name, $p.type, $default)
    }
    return ($parts -join ';')
}

function Get-ReplySignature($Parameters) {
    $parts = @()
    foreach ($p in @($Parameters)) {
        $parts += ('{0}|{1}' -f $p.name, $p.type)
    }
    return ($parts -join ';')
}

function Check-Api($Api, $Signatures, $Records) {
    Check-Equal $Api.schemaVersion 1 'api schemaVersion'
    $functions = @($Api.functions)
    Check-Equal $Api.functionCount 124 'api functionCount'
    Check-Equal $functions.Count 124 'api functions length'
    Check-Equal $Api.publicFunctionCount 86 'api publicFunctionCount'
    Check-Equal $Api.internalFunctionCount 38 'api internalFunctionCount'
    Check-Equal $Api.classificationCounts.public 86 'api classificationCounts.public'
    Check-Equal $Api.classificationCounts.internal 38 'api classificationCounts.internal'
    Check (@($Api.excludedScripts) -contains 'SLSF_Reloaded_MCM') 'api must explicitly exclude MCM'
    Check-Equal (@($functions | Select-Object -ExpandProperty id -Unique).Count) 124 'api unique function ids'
    Check-Equal $Signatures.schemaVersion 1 'signature oracle schemaVersion'
    Check-Sequence @($functions.id | Sort-Object) @($Signatures.functions.id | Sort-Object) 'api source function inventory'

    $publicCount = 0
    $internalCount = 0
    foreach ($f in $functions) {
        Check (-not [string]::IsNullOrWhiteSpace([string]$f.id)) 'api function id must not be empty'
        Check (-not [string]::IsNullOrWhiteSpace([string]$f.script)) "api function $($f.id) script"
        Check (-not [string]::IsNullOrWhiteSpace([string]$f.name)) "api function $($f.id) name"
        Check-Equal $f.id ("{0}.{1}" -f $f.script, $f.name) "api id for $($f.name)"
        $original = @($Signatures.functions | Where-Object { $_.id -ceq $f.id })[0]
        Check-Equal $f.scriptType $original.scriptType "api source script type for $($f.id)"
        Check-Equal $f.returnType $original.returnType "api source return type for $($f.id)"
        Check-Equal $f.isGlobal $original.isGlobal "api source global flag for $($f.id)"
        Check-Equal $f.isNative $original.isNative "api source native flag for $($f.id)"
        Check (@('Quest', 'ReferenceAlias', 'ActiveMagicEffect') -contains [string]$f.scriptType) "api scriptType for $($f.id)"
        Check (-not [string]::IsNullOrWhiteSpace([string]$f.returnType)) "api returnType for $($f.id)"
        Check ($f.isGlobal -is [bool]) "api isGlobal for $($f.id)"
        Check ($f.isNative -is [bool]) "api isNative for $($f.id)"
        Check ($f.isLatent -is [bool]) "api isLatent for $($f.id)"
        Check ($null -ne $f.PSObject.Properties['isPublic']) "api isPublic property for $($f.id)"
        $isPublic = [bool]$f.PSObject.Properties['isPublic'].Value
        Check (@('public', 'internal') -contains [string]$f.classification) "api classification for $($f.id)"
        Check-Equal $isPublic ([string]$f.classification -eq 'public') "api public flag for $($f.id)"
        if ($isPublic) { $publicCount++ } else { $internalCount++ }

        $parameters = @($f.parameters)
        Check-Equal $parameters.Count @($original.parameters).Count "api parameter count for $($f.id)"
        for ($i = 0; $i -lt $parameters.Count; $i++) {
            $expected = @($original.parameters)[$i]
            Check-Equal $parameters[$i].order $i "api parameter order for $($f.id)"
            Check-Equal $parameters[$i].name $expected.name "api parameter name for $($f.id)[$i]"
            Check-Equal $parameters[$i].type $expected.type "api parameter type for $($f.id)[$i]"
            Check-Equal ($null -eq $parameters[$i].default) ($null -eq $expected.default) "api parameter default presence for $($f.id)[$i]"
            Check-Equal $parameters[$i].default $expected.default "api parameter default for $($f.id)[$i]"
            Check (-not [string]::IsNullOrWhiteSpace([string]$parameters[$i].name)) "api parameter name for $($f.id)"
            Check (-not [string]::IsNullOrWhiteSpace([string]$parameters[$i].type)) "api parameter type for $($f.id)"
        }

        if ($isPublic) {
            Check ($null -ne $f.instance) "public function $($f.id) must have an instance binding"
            Check (-not [string]::IsNullOrWhiteSpace([string]$f.instance.kind)) "public function $($f.id) instance kind"
            if ($null -ne $f.instance.PSObject.Properties['formId']) {
                Check ([string]$f.instance.formId -match '^0x[0-9A-Fa-f]{8}$') "public function $($f.id) instance FormID"
            }
            Check (-not [string]::IsNullOrWhiteSpace([string]$f.invalidInputResult)) "public function $($f.id) invalidInputResult"
            if ($f.isGlobal) {
                $kind = 'global'
                if ($f.isNative) { $kind = 'global-native' }
                Check-Equal $f.instance.kind $kind "api global instance for $($f.id)"
            } elseif ($f.scriptType -eq 'Quest') {
                Check-Equal $f.instance.kind 'quest-script' "api quest instance for $($f.id)"
                $record = @($Records.records | Where-Object { $_.formId -eq $f.instance.formId })
                Check-Equal $record.Count 1 "api instance record for $($f.id)"
                Check (@($record[0].vmadScripts) -contains $f.script) "api instance VMAD script for $($f.id)"
                Check-Equal $record[0].signature 'QUST' "api instance record type for $($f.id)"
                Check-Equal $f.instance.editorId $record[0].editorId "api instance editor ID for $($f.id)"
            } elseif ($f.scriptType -eq 'ReferenceAlias') {
                $record = @($Records.records | Where-Object { $_.formId -eq $f.instance.questFormId })
                Check-Equal $record.Count 1 "api alias quest record for $($f.id)"
                $alias = @($record[0].vmad.aliases | Where-Object { $_.aliasIndex -eq $f.instance.aliasIndex })
                Check-Equal $alias.Count 1 "api alias binding for $($f.id)"
                Check (@($alias[0].scripts.name) -contains $f.script) "api alias VMAD script for $($f.id)"
            }
        }
    }
    Check-Equal $publicCount 86 'api public function count'
    Check-Equal $internalCount 38 'api internal function count'

    $gfl = @($functions | Where-Object { $_.id -eq 'SLSF_Reloaded_LocationManager.GetFameLocation' })
    Check-Equal $gfl.Count 1 'GetFameLocation entry'
    Check-Equal $gfl[0].invalidInputResult 'Null' 'GetFameLocation invalidInputResult'

    $aliasFunctions = @($functions | Where-Object { $_.scriptType -eq 'ReferenceAlias' })
    Check-Equal $aliasFunctions.Count 4 'ReferenceAlias public function count'
    foreach ($f in $aliasFunctions) {
        Check-Equal $f.instance.kind 'reference-alias' "alias instance kind for $($f.id)"
        Check-Equal $f.instance.questFormId '0x06000808' "alias quest FormID for $($f.id)"
        Check-Equal $f.instance.aliasIndex 0 "alias index for $($f.id)"
        Check-Equal $f.instance.aliasName 'PlayerAlias' "alias name for $($f.id)"
    }
}

function Check-Events($Events, $Cases) {
    Check-Equal $Events.schemaVersion 1 'events schemaVersion'
    Check-Equal $Events.inputEventCount 52 'events inputEventCount'
    Check-Equal $Events.replyEventCount 7 'events replyEventCount'
    Check-Equal $Events.categoryCount 25 'events categoryCount'

    $inputs = @($Events.inputEvents)
    $replies = @($Events.replyEvents)
    $categories = @($Events.categories)
    Check-Equal $inputs.Count 52 'events inputEvents length'
    Check-Equal $replies.Count 7 'events replyEvents length'
    Check-Equal $categories.Count 25 'events categories length'
    Check-Equal (@($inputs | Select-Object -ExpandProperty event -Unique).Count) 52 'events unique input event names'
    Check-Equal (@($inputs | Select-Object -ExpandProperty handler -Unique).Count) 52 'events unique handlers'

    $expectedInputs = @($Cases.inputEvents)
    Check-Equal $expectedInputs.Count 52 'cases inputEvents length'
    for ($i = 0; $i -lt $inputs.Count; $i++) {
        $actual = $inputs[$i]
        $expected = $expectedInputs[$i]
        Check-Equal $actual.index ($i + 1) "input event index $($actual.event)"
        Check-Equal $actual.event $expected.event "input event name at index $($i + 1)"
        Check-Equal $actual.handler $expected.handler "input handler for $($actual.event)"
        Check-Equal (Get-ParamSignature $actual.parameters) (Get-ParamSignature $expected.parameters) "input signature for $($actual.event)"
        Check (-not [string]::IsNullOrWhiteSpace([string]$actual.source)) "input source for $($actual.event)"
        $params = @($actual.parameters)
        for ($j = 0; $j -lt $params.Count; $j++) {
            Check-Equal $params[$j].order $j "input parameter order for $($actual.event)"
        }
    }

    $expectedReplies = @($Cases.replyEvents)
    Check-Equal $expectedReplies.Count 7 'cases replyEvents length'
    for ($i = 0; $i -lt $replies.Count; $i++) {
        $actual = $replies[$i]
        $expected = $expectedReplies[$i]
        Check-Equal $actual.index ($i + 1) "reply event index $($actual.event)"
        Check-Equal $actual.event $expected.event "reply event name at index $($i + 1)"
        Check-Equal (Get-ReplySignature $actual.parameters) (Get-ReplySignature $expected.parameters) "reply signature for $($actual.event)"
    }

    Check-Equal (@($replies | Select-Object -ExpandProperty event -Unique).Count) 7 'unique reply event names'
    Check-Equal (@($replies | Select-Object -ExpandProperty index -Unique).Count) 7 'unique reply indices'

    Check-Equal (@($categories | Select-Object -ExpandProperty name -Unique).Count) 25 'unique category names'
    Check-Equal (@($categories | Select-Object -ExpandProperty documentedIndex -Unique).Count) 25 'unique documented category indices'
    Check-Equal (@($categories | Select-Object -ExpandProperty espIndex -Unique).Count) 25 'unique ESP category indices'
    $documentedOrder = @($categories | Sort-Object { [int]$_.documentedIndex } | ForEach-Object { [string]$_.documentedIndex })
    $espIndexOrder = @($categories | Sort-Object { [int]$_.espIndex } | ForEach-Object { [string]$_.espIndex })
    Check-Sequence $documentedOrder (0..24) 'documented category index sequence'
    Check-Sequence $espIndexOrder (0..24) 'ESP category index sequence'
    Check-Sequence ($categories | ForEach-Object { [string]$_.name }) @($Cases.requiredCategories) 'category name oracle'
    $espOrder = @($categories | Sort-Object { [int]$_.espIndex } | ForEach-Object { [string]$_.espName })
    Check-Sequence $espOrder @($Cases.requiredEspOrder) 'category ESP order oracle'
    $tattoo = @($categories | Where-Object { $_.name -eq 'Tattoo' })
    $cumDump = @($categories | Where-Object { $_.name -eq 'Cum Dump' })
    Check-Equal $tattoo.Count 1 'Tattoo category'
    Check-Equal $cumDump.Count 1 'Cum Dump category'
    Check-Equal $tattoo[0].espName 'tattoo' 'Tattoo ESP spelling'
    Check-Equal $cumDump[0].espName 'cum dump' 'Cum Dump ESP spelling'
    Check-Sequence @($Events.nonCategoryFlags) @('Whore Event', 'Sex Worker') 'non-category flags'

    Check-Equal (@($Events.defaultLocations).Count) 21 'default locations length'
    Check-Equal (@($Events.defaultLocations | Select-Object -Unique).Count) 21 'unique default locations'
    Check-Sequence @($Events.defaultLocations) @($Cases.defaultLocations) 'default location oracle'
    Check-Equal (@($Events.locationAliases).Count) 6 'location aliases length'
    Check-Sequence ($Events.locationAliases | ForEach-Object { "$($_.alias)->$($_.canonical)" }) ($Cases.locationAliases | ForEach-Object { "$($_.alias)->$($_.canonical)" }) 'location alias oracle'
    foreach ($alias in @($Events.locationAliases)) {
        Check (@($Events.defaultLocations) -contains [string]$alias.canonical) "location alias $($alias.alias) target"
    }

    $queryNames = @($Events.queryReplies.PSObject.Properties | ForEach-Object { $_.Name })
    Check-Equal $queryNames.Count 7 'queryReplies count'
    $expectedQueryNames = @('SLSF_Reloaded_RequestLocation', 'SLSF_Reloaded_RequestRecognitionTime', 'SLSF_Reloaded_RequestFame', 'SLSF_Reloaded_RequestCumVisibility', 'SLSF_Reloaded_RequestFlagState', 'SLSF_Reloaded_RequestModFlagState', 'SLSF_Reloaded_RequestModRegisterState')
    Check-Sequence $queryNames $expectedQueryNames 'queryReplies keys'
    foreach ($name in $queryNames) {
        $replyNames = @($Events.queryReplies.$name)
        Check-Equal $replyNames.Count 1 "queryReplies $name count"
        Check (@($replies | ForEach-Object { $_.event }) -contains [string]$replyNames[0]) "queryReplies $name target"
        Check (@($inputs | ForEach-Object { $_.event }) -contains [string]$name) "queryReplies $name input"
    }
    $queryTargets = @($queryNames | ForEach-Object { [string]$Events.queryReplies.$_ })
    Check-Equal (@($queryTargets | Select-Object -Unique).Count) 7 'queryReplies unique targets'
    foreach ($reply in $replies) {
        Check ($queryTargets -contains [string]$reply.event) "reply $($reply.event) must have a query mapping"
    }
    Check (-not [string]::IsNullOrWhiteSpace([string]$Events.invalidInputBehavior)) 'invalidInputBehavior'
}

function Check-Records($Records, $Cases) {
    Check-Equal $Records.schemaVersion 1 'records schemaVersion'
    Check-Equal $Records.recordCount 58 'records recordCount'
    $recordList = @($Records.records)
    Check-Equal $recordList.Count 58 'records length'
    Check-Equal (@($recordList | Select-Object -ExpandProperty formId -Unique).Count) 58 'unique record FormIDs'
    Check-Equal (@($recordList | Select-Object -ExpandProperty editorId -Unique).Count) 58 'unique record editorIDs'
    Check-Equal $Records.signatureCounts.QUST 4 'records QUST count'
    Check-Equal $Records.signatureCounts.MGEF 1 'records MGEF count'
    Check-Equal $Records.signatureCounts.KYWD 10 'records KYWD count'
    Check-Equal $Records.signatureCounts.GLOB 42 'records GLOB count'
    Check-Equal $Records.signatureCounts.SPEL 1 'records SPEL count'
    $sum = 0
    foreach ($p in $Records.signatureCounts.PSObject.Properties) { $sum += [int]$p.Value }
    Check-Equal $sum 58 'records signature count sum'
    $publicCount = @($recordList | Where-Object { $_.isPublic -eq $true }).Count
    Check-Equal $publicCount 57 'public record count'
    Check-Equal $Records.publicRecordCount 57 'records publicRecordCount'

    $expectedRecords = @($Cases.records)
    Check-Equal $expectedRecords.Count 58 'cases record length'
    $allowedSignatures = @('QUST', 'MGEF', 'KYWD', 'GLOB', 'SPEL')
    for ($i = 0; $i -lt $recordList.Count; $i++) {
        $r = $recordList[$i]
        $e = $expectedRecords[$i]
        Check ($allowedSignatures -contains [string]$r.signature) "record signature $($r.editorId)"
        Check ([string]$r.formId -match '^0x[0-9A-Fa-f]{8}$') "record FormID $($r.editorId)"
        Check (-not [string]::IsNullOrWhiteSpace([string]$r.editorId)) "record editorID $i"
        Check-Equal $r.plugin 'SLSF Reloaded.esp' "record plugin $($r.editorId)"
        Check-Equal $r.signature $e.signature "record signature oracle $i"
        Check-Equal $r.formId $e.formId "record FormID oracle $($r.editorId)"
        Check-Equal $r.editorId $e.editorId "record editorID oracle $i"
        Check-Equal $r.recordFlags '0x00000000' "record header flags $($r.editorId)"
        Check-Equal ([bool]$r.isPublic) ([bool]$e.isPublic) "record public flag $($r.editorId)"
        $actualVmad = @()
        if ($null -ne $r.PSObject.Properties['vmadScripts']) { $actualVmad = @($r.vmadScripts) }
        Check-Sequence $actualVmad @($e.vmadScripts) "record VMAD scripts $($r.editorId)"
        if ([string]$r.signature -eq 'QUST') {
            Check-Equal $r.questFlags $e.questFlags "record quest flags $($r.editorId)"
            Check-Equal $r.questFlags '0x00000111' "record quest flags value $($r.editorId)"
        } else {
            Check (@($r.PSObject.Properties | Where-Object { $_.Name -eq 'questFlags' }).Count -eq 0) "non-quest record has no questFlags $($r.editorId)"
        }
        if ($null -ne $r.PSObject.Properties['vmad']) {
            Check-Sequence @($r.vmad.scripts | ForEach-Object { $_.name }) @($r.vmadScripts) "record VMAD script details $($r.editorId)"
        } else {
            Check-Equal $actualVmad.Count 0 "non-VMAD record has no scripts $($r.editorId)"
        }
    }

    $main = @($recordList | Where-Object { $_.editorId -eq 'SLSF_Reloaded_Main' })
    Check-Equal $main.Count 1 'Main quest record'
    Check-Sequence @($main[0].vmadScripts) @('SLSF_Reloaded_ModEventListener', 'SLSF_Reloaded_FameManager', 'SLSF_Reloaded_ModIntegration', 'SLSF_Reloaded_Logger') 'Main quest VMAD scripts'
    Check-Equal $main[0].vmad.fragments.unknown 2 'Main VMAD unknown field'
    Check-Equal $main[0].vmad.fragments.fragmentCount 0 'Main VMAD fragment count'
    Check-Equal $main[0].vmad.aliasCount 1 'Main VMAD alias count'
    Check-Equal (@($main[0].vmad.aliases).Count) 1 'Main VMAD aliases length'
    $alias = @($main[0].vmad.aliases)[0]
    Check-Equal $alias.aliasIndex 0 'Main alias index'
    Check-Equal $alias.aliasName 'PlayerAlias' 'Main alias name'
    Check-Equal $alias.questFormId '0x06000808' 'Main alias quest FormID'
    Check-Sequence @($alias.scripts | ForEach-Object { $_.name }) @('SLSF_Reloaded_SexAnimationAnalyzer', 'SLSF_Reloaded_PlayerScript') 'Main alias scripts'
    Check-Equal (@($alias.scripts)[0].properties).Count 14 'SexAnimationAnalyzer alias property count'
    Check-Equal (@($alias.scripts)[1].properties).Count 17 'PlayerScript alias property count'
    $mainCase = @($expectedRecords | Where-Object { $_.editorId -eq 'SLSF_Reloaded_Main' })
    Check-Equal $mainCase.Count 1 'cases Main quest record'
    Check-Equal (@($mainCase[0].aliases).Count) 1 'cases Main alias length'
    Check-Sequence @($mainCase[0].aliases[0].scripts | ForEach-Object { $_.name }) @('SLSF_Reloaded_SexAnimationAnalyzer', 'SLSF_Reloaded_PlayerScript') 'cases Main alias scripts'
    Check-Equal (@($mainCase[0].aliases[0].scripts)[0].propertyCount) 14 'cases SexAnimationAnalyzer property count'
    Check-Equal (@($mainCase[0].aliases[0].scripts)[1].propertyCount) 17 'cases PlayerScript property count'
    $managers = @($recordList | Where-Object { $_.editorId -eq 'SLSF_Reloaded_Managers' })
    Check-Equal $managers.Count 1 'Managers quest record'
    Check-Equal (@($managers[0].vmadScripts).Count) 9 'Managers VMAD script count'
    Check-Sequence @($managers[0].vmadScripts) @('SLSF_Reloaded_CommentManager', 'SLSF_Reloaded_VisibilityManager', 'SLSF_Reloaded_DataManager', 'SLSF_Reloaded_LegacyOverwrite', 'SLSF_Reloaded_Data_Exporter', 'SLSF_Reloaded_Data_Importer', 'SLSF_Reloaded_LocationManager', 'SLSF_Reloaded_Uninstall', 'SLSF_Reloaded_DynamicAnonymity') 'Managers VMAD script oracle'
    $mcm = @($recordList | Where-Object { $_.editorId -eq 'SLSF_Reloaded_MCM' })
    Check-Equal $mcm.Count 1 'MCM quest record'
    Check-Sequence @($mcm[0].vmadScripts) @('SLSF_Reloaded_MCM') 'MCM VMAD scripts'
    $clock = @($recordList | Where-Object { $_.editorId -eq 'SLSF_Reloaded_MasterClock' })
    Check-Equal $clock.Count 1 'MasterClock quest record'
    Check-Sequence @($clock[0].vmadScripts) @('SLSF_Reloaded_MasterClock') 'MasterClock VMAD scripts'
    $effect = @($recordList | Where-Object { $_.editorId -eq 'SLSF_Reloaded_NPCScanEffect' })
    Check-Equal $effect.Count 1 'NPCScan effect record'
    Check-Sequence @($effect[0].vmadScripts) @('SLSF_Reloaded_NPCScan') 'NPCScan effect VMAD scripts'
}

function Check-Rules($Rules, $Events) {
    Check-Equal $Rules.schemaVersion 1 'rules schemaVersion'
    $categories = @($Rules.categories)
    $ruleList = @($Rules.rules)
    Check-Equal $categories.Count 25 'rules categories length'
    Check-Equal $ruleList.Count 23 'rules rule count'
    Check-Equal (@($ruleList | Select-Object -ExpandProperty id -Unique).Count) 23 'rules unique ids'
    Check-Equal $Rules.counts.categories 25 'rules counts.categories'
    Check-Equal $Rules.counts.defaultLocations 21 'rules counts.defaultLocations'
    Check-Equal $Rules.counts.inputEvents 52 'rules counts.inputEvents'
    Check-Equal $Rules.counts.replyEvents 7 'rules counts.replyEvents'
    Check-Equal $Rules.counts.functions 124 'rules counts.functions'
    Check-Equal $Rules.counts.records 58 'rules counts.records'
    Check-Equal $Rules.counts.rules 23 'rules counts.rules'
    Check-Equal (@($categories | Where-Object { $_.optionalCleanup -eq $true }).Count) 4 'optional cleanup category count'
    $optionalNames = @($categories | Where-Object { $_.optionalCleanup -eq $true } | ForEach-Object { [string]$_.name })
    Check-Sequence $optionalNames @('Cum Dump', 'Unfaithful', 'Cuck', 'Airhead') 'optional cleanup categories'
    $eventCategories = @($Events.categories)
    Check-Equal $eventCategories.Count 25 'events categories for rules'
    for ($i = 0; $i -lt $categories.Count; $i++) {
        Check-Equal $categories[$i].name $eventCategories[$i].name "rules category name $i"
        Check-Equal $categories[$i].espName $eventCategories[$i].espName "rules category ESP name $i"
        Check-Equal $categories[$i].documentedIndex $eventCategories[$i].documentedIndex "rules category documented index $i"
        Check-Equal $categories[$i].espIndex $eventCategories[$i].espIndex "rules category ESP index $i"
    }
    $aliases = @($Rules.categoryAliases)
    Check-Equal $aliases.Count 2 'rules category alias count'
    Check-Sequence ($aliases | ForEach-Object { "$($_.alias)->$($_.canonical)" }) @('tattoo->Tattoo', 'cum dump->Cum Dump') 'rules category aliases'
    $nonCategory = @($Rules.nonCategoryFlags)
    Check-Equal $nonCategory.Count 2 'rules non-category flag count'
    Check-Sequence ($nonCategory | ForEach-Object { [string]$_.flagName }) @('Whore Event', 'Sex Worker') 'rules non-category flags'
    Check-Equal $Rules.numeric.maxFameValue 150 'rules maxFameValue'
    Check-Equal $Rules.numeric.manualSpreadMinSource 10 'rules manualSpreadMinSource'
    Check-Equal $Rules.numeric.manualSpreadMinPercent 10 'rules manualSpreadMinPercent'
    Check-Equal $Rules.numeric.manualSpreadMaxPercent 100 'rules manualSpreadMaxPercent'
    Check-Equal $Rules.numeric.setCommandClamp $false 'rules setCommandClamp'
    Check-Equal $Rules.numeric.slotCount 32 'rules slotCount'
    Check-Equal $Rules.numeric.avx2Lanes 4 'rules avx2Lanes'
    Check-Equal $Rules.numeric.avx2VectorsPerRow 8 'rules avx2VectorsPerRow'
    Check-Equal $Rules.numeric.paddingSlots 7 'rules paddingSlots'
    Check-Equal $Rules.numeric.autoSpreadBatchCategories 7 'rules spread batch'
    Check-Equal $Rules.externalFlags.aggregation 'OR' 'rules external flag aggregation'
    Check-Equal $Rules.externalFlags.unknownFlagResult $false 'rules unknown flag result'
    Check-Equal $Rules.externalFlags.registerRequiresGameMod $true 'rules register requires Game mod'
    Check-Equal $Rules.externalFlags.countExcludesEmpty $true 'rules count excludes empty'
    Check-Equal $Rules.persistence.authority 'C++ PersistentState' 'rules persistence authority'
    Check-Equal @($Rules.locations.defaultOrder).Count 21 'rules location order length'
    Check-Sequence @($Rules.locations.defaultOrder) @($Events.defaultLocations) 'rules location order oracle'
    Check-Sequence @($Rules.locations.majorIndices) (0..8) 'rules major indices'
    Check-Sequence @($Rules.locations.minorIndices) (9..20) 'rules minor indices'

    $requiredIds = @('categories-25', 'event-input-52', 'event-reply-7', 'location-default-21', 'manual-spread-formula', 'auto-spread-pulse', 'published-max', 'wipe-once', 'record-identities', 'mcm-excluded', 'new-save-only')
    foreach ($id in $requiredIds) {
        $matches = @($ruleList | Where-Object { $_.id -eq $id })
        Check-Equal $matches.Count 1 "required rule $id"
    }
    $preserve = 0
    $approved = 0
    $internal = 0
    foreach ($rule in $ruleList) {
        Check (@('preserve', 'approved-change', 'internal-only') -contains [string]$rule.classification) "rule classification $($rule.id)"
        Check (-not [string]::IsNullOrWhiteSpace([string]$rule.statement)) "rule statement $($rule.id)"
        Check (@($rule.source).Count -gt 0) "rule source $($rule.id)"
        switch ([string]$rule.classification) {
            'preserve' { $preserve++ }
            'approved-change' {
                $approved++
                Check (-not [string]::IsNullOrWhiteSpace([string]$rule.approvedChange)) "approved change reason $($rule.id)"
            }
            'internal-only' { $internal++ }
        }
    }
    Check-Equal $preserve $Rules.counts.preserveRules 'rules preserved count'
    Check-Equal $approved $Rules.counts.approvedChangeRules 'rules approved count'
    Check-Equal $internal $Rules.counts.internalOnlyRules 'rules internal count'
}

function Check-All($Documents) {
    Check-Source $Documents.Api $Documents.Records
    Check-Records $Documents.Records $Documents.Cases
    Check-Api $Documents.Api $Documents.Signatures $Documents.Records
    Check-Events $Documents.Events $Documents.Cases
    Check-Rules $Documents.Rules $Documents.Events
}

function Apply-Mutation($Document, $Mutation) {
    switch ([string]$Mutation.operation) {
        'removeInputEvent' {
            $before = @($Document.inputEvents).Count
            $Document.inputEvents = @($Document.inputEvents | Where-Object { $_.event -ne [string]$Mutation.selector })
            if (@($Document.inputEvents).Count -eq $before) { Fail "negative fixture selector not found: $($Mutation.selector)" }
        }
        'swapInputEventParameters' {
            $matches = @($Document.inputEvents | Where-Object { $_.event -eq [string]$Mutation.selector })
            if ($matches.Count -ne 1) { Fail "negative fixture event not found: $($Mutation.selector)" }
            $params = @($matches[0].parameters)
            $left = [int]$Mutation.indices[0]
            $right = [int]$Mutation.indices[1]
            $tmp = $params[$left]
            $params[$left] = $params[$right]
            $params[$right] = $tmp
            $matches[0].parameters = $params
        }
        'replaceCategoryName' {
            $matches = @($Document.categories | Where-Object { $_.name -eq [string]$Mutation.selector })
            if ($matches.Count -ne 1) { Fail "negative fixture category not found: $($Mutation.selector)" }
            $matches[0].name = [string]$Mutation.replacement
            $matches[0].espName = [string]$Mutation.replacement
        }
        'invalidateRecordIdentity' {
            $matches = @($Document.records | Where-Object { $_.editorId -eq [string]$Mutation.selector })
            if ($matches.Count -ne 1) { Fail "negative fixture record not found: $($Mutation.selector)" }
            $matches[0].formId = [string]$Mutation.formId
            $matches[0].plugin = [string]$Mutation.plugin
        }
        'replaceInvalidInputResult' {
            $matches = @($Document.functions | Where-Object { $_.id -eq [string]$Mutation.selector })
            if ($matches.Count -ne 1) { Fail "negative fixture function not found: $($Mutation.selector)" }
            $matches[0].invalidInputResult = [string]$Mutation.replacement
        }
        'changeFunctionField' {
            $matches = @($Document.functions | Where-Object { $_.id -eq [string]$Mutation.selector })
            if ($matches.Count -ne 1) { Fail "negative fixture function not found: $($Mutation.selector)" }
            $function = $matches[0]
            switch ([string]$Mutation.field) {
                'parameterType' { $function.parameters[[int]$Mutation.index].type = $Mutation.replacement }
                'parameterDefault' { $function.parameters[[int]$Mutation.index].default = $Mutation.replacement }
                'returnType' { $function.returnType = $Mutation.replacement }
                'isGlobal' { $function.isGlobal = $Mutation.replacement }
                'instanceFormId' { $function.instance.formId = $Mutation.replacement }
                default { Fail "unknown negative function field: $($Mutation.field)" }
            }
        }
        'reverseVmadScripts' {
            $matches = @($Document.records | Where-Object { $_.editorId -eq [string]$Mutation.selector })
            if ($matches.Count -ne 1) { Fail "negative fixture record not found: $($Mutation.selector)" }
            $scripts = @($matches[0].vmadScripts)
            [array]::Reverse($scripts)
            $matches[0].vmadScripts = $scripts
        }
        default {
            Fail "unknown negative fixture operation: $($Mutation.operation)"
        }
    }
}

function Invoke-NegativeFixtures {
    $negativeDir = Join-Path $fixturesRoot 'negative'
    $fixtures = @(Get-ChildItem -LiteralPath $negativeDir -Filter '*.json' -File | Sort-Object Name)
    $expectedCases = @(
        'api-default',
        'api-global-flag',
        'api-instance-binding',
        'api-parameter-type',
        'api-return-type',
        'delete-one-input-event',
        'duplicate-category',
        'invalid-api-result',
        'invalid-record-identity',
        'managers-vmad-reversal',
        'swap-event-parameters'
    )
    Check-Equal $fixtures.Count $expectedCases.Count 'negative fixture count'
    Check-Sequence @($fixtures | ForEach-Object { $_.BaseName }) $expectedCases 'negative fixture set'
    foreach ($fixture in $fixtures) {
        $mutation = Read-Json $fixture.FullName
        Check-Equal $mutation.schemaVersion 1 "negative fixture schema $($fixture.Name)"
        Check-Equal $mutation.expected 'rejected' "negative fixture expectation $($mutation.case)"
        Check (-not [string]::IsNullOrWhiteSpace([string]$mutation.reason)) "negative fixture reason $($mutation.case)"
        Check (-not [string]::IsNullOrWhiteSpace([string]$mutation.expectedFailure)) "negative fixture expectedFailure $($mutation.case)"
        $documents = Load-Documents
        switch ([string]$mutation.target) {
            'contracts/legacy-api.json' { Apply-Mutation $documents.Api $mutation }
            'contracts/legacy-events.json' { Apply-Mutation $documents.Events $mutation }
            'contracts/legacy-records.json' { Apply-Mutation $documents.Records $mutation }
            default { Fail "negative fixture targets unsupported document: $($mutation.target)" }
        }
        $rejected = $false
        $reason = ''
        try {
            Check-All $documents
        }
        catch {
            $rejected = $true
            $reason = $_.Exception.Message
        }
        if (-not $rejected) { Fail "negative case '$($mutation.case)' was accepted" }
        if ($reason -notlike "*$($mutation.expectedFailure)*") {
            Fail ("negative case '{0}' rejected for the wrong reason: expected '{1}', got '{2}'" -f $mutation.case, $mutation.expectedFailure, $reason)
        }
        $script:checkedNegative++
        Write-Host ("REJECT  {0}: {1}" -f $mutation.case, $reason)
    }
    Write-Host ("PASS negative fixtures: {0}/{1} rejected" -f $script:checkedNegative, $fixtures.Count)
}

try {
    $documents = Load-Documents
    Check-All $documents
    Write-Host 'PASS contracts: 124 functions (86 public), 52 inputs, 7 replies, 25 categories, 58 records, 23 rules'
    if ($NegativeFixtures) {
        Invoke-NegativeFixtures
    }
    exit 0
}
catch {
    [Console]::Error.WriteLine("CONTRACT-VALIDATION-FAILED: $($_.Exception.Message)")
    [Console]::Error.WriteLine($_.ScriptStackTrace)
    exit 1
}
