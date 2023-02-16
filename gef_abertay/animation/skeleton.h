#ifndef _SKELETON_H
#define _SKELETON_H

#include <gef.h>
#include <system/string_id.h>
#include <maths/matrix44.h>
#include <animation/joint.h>
#include <system/vec.h>

#include <ostream>
#include <istream>

namespace gef {
	struct Joint;

	class Skeleton {
	public:
		Int32 AddJoint(const Joint &joint);
		Int32 FindJointIndex(const StringId joint_name) const;
		const Joint *FindJoint(const StringId joint_name) const;

		bool Read(std::istream &stream);
		bool Write(std::ostream &stream) const;

		inline Int32 joint_count() const { return (Int32)joints_.size(); }
		inline const Joint &joint(const Int32 index) const { return joints_[index]; }
		inline Joint &joint(const Int32 index) {
			return const_cast<Joint &>(static_cast<const Skeleton &>(*this).joint(index));
		}

		inline const gef::Vec<Joint> &joints() const { return joints_; }
		inline gef::Vec<Joint> &joints() {
			return const_cast<gef::Vec<Joint>&>(static_cast<const Skeleton &>(*this).joints());
		}
	private:
		gef::Vec<Joint> joints_;
	};

	class SkeletonPose {
	public:
		SkeletonPose(IAllocator *allocator = g_alloc);
		void CalculateGlobalPose(const gef::Matrix44 *const pose_transform = NULL);
		void CalculateLocalPose(const gef::Vec<Matrix44> &global_pose);
		void SetPoseFromAnim(const class Animation &_anim, const SkeletonPose &_bindPose, const float _time, const bool _updateGlobalPose = true);
	//	void SetLocalJointPoseFromAnim(JointPose& _jointPose, const UInt32 _jointNum, const JointPose& _jointBindPose, const class Anim& _anim, const float _time);
		static SkeletonPose lerp(const SkeletonPose &start, const SkeletonPose &end, float time);
		void Linear2PoseBlend(const SkeletonPose &_startPose, const SkeletonPose &_endPose, const float _time);

		static gef::Matrix44 GetGlobalJointTransformFromAnim(const class Animation *_anim, const SkeletonPose &_bindPose, float _time, const Int32 joint_index);
		static gef::Matrix44 GetJointTransformFromAnim(const class Animation &_anim, const SkeletonPose &_bindPose, float _time, const Int32 joint_index);

		void CreateBindPose(const Skeleton *const skeleton);
		void CleanUp();

		inline gef::Vec<JointPose> &local_pose() { return local_pose_; }
		inline gef::Vec<Matrix44> &global_pose() { return global_pose_; }

		inline const gef::Vec<JointPose> &local_pose() const { return local_pose_; }
		inline const gef::Vec<Matrix44> &global_pose() const { return global_pose_; }
		inline void setSkeleton(const Skeleton *sk) { skeleton_ = sk; }
		inline const Skeleton *skeleton() const { return skeleton_; }

	private:
		gef::Vec<JointPose>	local_pose_;	// local joint poses
		gef::Vec<Matrix44> global_pose_;	// global joint poses
		const Skeleton *skeleton_;
	};
}
#endif // _SKELETON_H
