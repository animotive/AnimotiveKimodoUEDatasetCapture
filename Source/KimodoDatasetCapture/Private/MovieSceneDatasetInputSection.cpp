// Copyright Animotive. All Rights Reserved.

#include "MovieSceneDatasetInputSection.h"

#include "Channels/MovieSceneChannelProxy.h"
#include "KimodoDatasetCaptureLog.h"

#define LOCTEXT_NAMESPACE "MovieSceneDatasetInputSection"

UMovieSceneDatasetInputSection::UMovieSceneDatasetInputSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A capture always writes the whole range it recorded, so the section starts empty and
	// takes its range from the data in WriteSamples.
	SetRange(TRange<FFrameNumber>::Empty());

	RecreateChannelProxy();
}

void UMovieSceneDatasetInputSection::WriteSample(const FDatasetInputSample& Sample)
{
	const FFrameNumber Frame(Sample.FrameNumber);

	// AddConstantKey, not AddLinearKey or AddCubicKey. Input must never be interpolated:
	// half a button press does not exist, and a stick value invented between two samples is
	// data the hardware never reported.
	LeftStickX.AddConstantKey(Frame, Sample.LeftStickX);
	LeftStickY.AddConstantKey(Frame, Sample.LeftStickY);
	RightStickX.AddConstantKey(Frame, Sample.RightStickX);
	RightStickY.AddConstantKey(Frame, Sample.RightStickY);
	LeftTrigger.AddConstantKey(Frame, Sample.LeftTrigger);
	RightTrigger.AddConstantKey(Frame, Sample.RightTrigger);

	// The bitmask is stored as int32 on the wire of the channel. The cast is value-preserving
	// for the fourteen bits currently defined; a wider button set would need a second channel
	// rather than a reinterpretation of this one.
	Buttons.GetData().AddKey(Frame, static_cast<int32>(Sample.Buttons));

	// The keyboard mask and mouse delta are their own channels. Folding them into the gamepad
	// channels would make a keyboard press indistinguishable from a pad press in the recorded
	// data, which is precisely what the separate-device decision rules out.
	KeyboardButtons.GetData().AddKey(Frame, static_cast<int32>(Sample.KeyboardButtons));
	MouseDeltaX.AddConstantKey(Frame, Sample.MouseDeltaX);
	MouseDeltaY.AddConstantKey(Frame, Sample.MouseDeltaY);
}

void UMovieSceneDatasetInputSection::WriteSamples(TArrayView<const FDatasetInputSample> Samples)
{
	if (Samples.IsEmpty())
	{
		return;
	}

	for (const FDatasetInputSample& Sample : Samples)
	{
		WriteSample(Sample);
	}

	// The range must cover every key, or the section draws wrong in Sequencer and can be
	// evaluated out of existence. The upper bound is exclusive, so it sits one frame past the
	// last sample in order to contain it.
	const FFrameNumber FirstFrame(Samples[0].FrameNumber);
	const FFrameNumber LastFrame(Samples[Samples.Num() - 1].FrameNumber);

	const TRange<FFrameNumber> SampleRange(FirstFrame, LastFrame + 1);
	SetRange(GetRange().IsEmpty() ? SampleRange : TRange<FFrameNumber>::Hull(GetRange(), SampleRange));
}

bool UMovieSceneDatasetInputSection::TryReadSample(FFrameNumber FrameNumber, FDatasetInputSample& OutSample) const
{
	// Sample presence is decided by an EXACT key on the button channel, with zero tolerance.
	// Every sample writes a key on every channel, including a zero bitmask, so this channel
	// is authoritative for "was this frame sampled".
	//
	// Without this check the channels would happily evaluate an unsampled frame to the held
	// value of the previous key, and the caller could not tell a recorded frame from an
	// invented one.
	const TArrayView<const FFrameNumber> ButtonTimes = Buttons.GetData().GetTimes();
	const int32 KeyIndex = Algo::LowerBound(ButtonTimes, FrameNumber);
	if (!ButtonTimes.IsValidIndex(KeyIndex) || ButtonTimes[KeyIndex] != FrameNumber)
	{
		return false;
	}

	const FFrameTime FrameTime(FrameNumber);

	FDatasetInputSample Sample;
	Sample.FrameNumber = FrameNumber.Value;

	LeftStickX.Evaluate(FrameTime, Sample.LeftStickX);
	LeftStickY.Evaluate(FrameTime, Sample.LeftStickY);
	RightStickX.Evaluate(FrameTime, Sample.RightStickX);
	RightStickY.Evaluate(FrameTime, Sample.RightStickY);
	LeftTrigger.Evaluate(FrameTime, Sample.LeftTrigger);
	RightTrigger.Evaluate(FrameTime, Sample.RightTrigger);

	int32 ButtonValue = 0;
	Buttons.Evaluate(FrameTime, ButtonValue);
	Sample.Buttons = static_cast<uint32>(ButtonValue);

	int32 KeyboardValue = 0;
	KeyboardButtons.Evaluate(FrameTime, KeyboardValue);
	Sample.KeyboardButtons = static_cast<uint32>(KeyboardValue);

	MouseDeltaX.Evaluate(FrameTime, Sample.MouseDeltaX);
	MouseDeltaY.Evaluate(FrameTime, Sample.MouseDeltaY);

	OutSample = Sample;
	return true;
}

int32 UMovieSceneDatasetInputSection::GetSampleCount() const
{
	return Buttons.GetData().GetTimes().Num();
}

TArray<FDatasetInputSample> UMovieSceneDatasetInputSection::ReadAllSamples() const
{
	const TArrayView<const FFrameNumber> ButtonTimes = Buttons.GetData().GetTimes();

	TArray<FDatasetInputSample> Samples;
	Samples.Reserve(ButtonTimes.Num());

	for (const FFrameNumber& Time : ButtonTimes)
	{
		FDatasetInputSample Sample;
		if (TryReadSample(Time, Sample))
		{
			Samples.Add(Sample);
		}
		else
		{
			// A key time that does not read back means the channels disagree about which
			// frames were sampled, which would corrupt the sidecar derived from them.
			UE_LOG(
				LogKimodoDatasetCapture,
				Error,
				TEXT("Input section has a key at frame %d that could not be read back. The section is inconsistent."),
				Time.Value);
		}
	}

	return Samples;
}

void UMovieSceneDatasetInputSection::RecreateChannelProxy()
{
	FMovieSceneChannelProxyData Channels;

#if WITH_EDITOR

	// Metadata drives how the channels appear in Sequencer. Sort order matches the physical
	// pad layout (left stick, right stick, triggers, buttons) rather than declaration order,
	// so an operator reading the timeline sees something shaped like the controller.
	FMovieSceneChannelMetaData LeftStickXMetaData(TEXT("LeftStickX"), LOCTEXT("LeftStickX", "Left Stick X"));
	LeftStickXMetaData.SortOrder = 0;
	Channels.Add(LeftStickX, LeftStickXMetaData, TMovieSceneExternalValue<float>());

	FMovieSceneChannelMetaData LeftStickYMetaData(TEXT("LeftStickY"), LOCTEXT("LeftStickY", "Left Stick Y"));
	LeftStickYMetaData.SortOrder = 1;
	Channels.Add(LeftStickY, LeftStickYMetaData, TMovieSceneExternalValue<float>());

	FMovieSceneChannelMetaData RightStickXMetaData(TEXT("RightStickX"), LOCTEXT("RightStickX", "Right Stick X"));
	RightStickXMetaData.SortOrder = 2;
	Channels.Add(RightStickX, RightStickXMetaData, TMovieSceneExternalValue<float>());

	FMovieSceneChannelMetaData RightStickYMetaData(TEXT("RightStickY"), LOCTEXT("RightStickY", "Right Stick Y"));
	RightStickYMetaData.SortOrder = 3;
	Channels.Add(RightStickY, RightStickYMetaData, TMovieSceneExternalValue<float>());

	FMovieSceneChannelMetaData LeftTriggerMetaData(TEXT("LeftTrigger"), LOCTEXT("LeftTrigger", "Left Trigger"));
	LeftTriggerMetaData.SortOrder = 4;
	Channels.Add(LeftTrigger, LeftTriggerMetaData, TMovieSceneExternalValue<float>());

	FMovieSceneChannelMetaData RightTriggerMetaData(TEXT("RightTrigger"), LOCTEXT("RightTrigger", "Right Trigger"));
	RightTriggerMetaData.SortOrder = 5;
	Channels.Add(RightTrigger, RightTriggerMetaData, TMovieSceneExternalValue<float>());

	FMovieSceneChannelMetaData ButtonsMetaData(TEXT("Buttons"), LOCTEXT("Buttons", "Gamepad Buttons (bitmask)"));
	ButtonsMetaData.SortOrder = 6;
	Channels.Add(Buttons, ButtonsMetaData, TMovieSceneExternalValue<int32>());

	FMovieSceneChannelMetaData MouseDeltaXMetaData(TEXT("MouseDeltaX"), LOCTEXT("MouseDeltaX", "Mouse Delta X"));
	MouseDeltaXMetaData.SortOrder = 7;
	Channels.Add(MouseDeltaX, MouseDeltaXMetaData, TMovieSceneExternalValue<float>());

	FMovieSceneChannelMetaData MouseDeltaYMetaData(TEXT("MouseDeltaY"), LOCTEXT("MouseDeltaY", "Mouse Delta Y"));
	MouseDeltaYMetaData.SortOrder = 8;
	Channels.Add(MouseDeltaY, MouseDeltaYMetaData, TMovieSceneExternalValue<float>());

	FMovieSceneChannelMetaData KeyboardMetaData(TEXT("KeyboardButtons"), LOCTEXT("KeyboardButtons", "Keyboard/Mouse Buttons (bitmask)"));
	KeyboardMetaData.SortOrder = 9;
	Channels.Add(KeyboardButtons, KeyboardMetaData, TMovieSceneExternalValue<int32>());

#else

	Channels.Add(LeftStickX);
	Channels.Add(LeftStickY);
	Channels.Add(RightStickX);
	Channels.Add(RightStickY);
	Channels.Add(LeftTrigger);
	Channels.Add(RightTrigger);
	Channels.Add(Buttons);
	Channels.Add(MouseDeltaX);
	Channels.Add(MouseDeltaY);
	Channels.Add(KeyboardButtons);

#endif

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(MoveTemp(Channels));
}

void UMovieSceneDatasetInputSection::PostEditImport()
{
	Super::PostEditImport();
	RecreateChannelProxy();
}

void UMovieSceneDatasetInputSection::PostLoad()
{
	Super::PostLoad();
	RecreateChannelProxy();
}

#undef LOCTEXT_NAMESPACE
